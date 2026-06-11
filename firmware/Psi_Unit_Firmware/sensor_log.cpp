/*
 * sensor_log.cpp — write measurement history to flash files
 *
 * Three ring-buffer files: INC temperature, GRH temperature, GRH humidity.
 * enabled and interval parameters are stored in NVS namespace "psi_log"
 * (separate from settings).
 */

#include "sensor_log.h"

#if SENSOR_LOG_ENABLE && defined(ARDUINO_ARCH_ESP32) && !defined(SIM_BUILD)

#include "app_snapshot.h"
#include "rtc_clock.h"
#include "app_platform.h"
#include "wifi_json_buf.h"
#include <FS.h>
#include <LittleFS.h>
#include <FFat.h>
#include <Preferences.h>
#include <esp_partition.h>
#include <stdio.h>
#include <string.h>

static const char *LOG_NVS_NS = "psi_log";
static const char *LOG_KEY_ENABLED = "en";
static const char *LOG_KEY_INTERVAL = "intMin";

static const char *LOG_FILE_PATHS[SENSOR_LOG_COUNT] = {
    "/sl_inc.bin",
    "/sl_grh_t.bin",
    "/sl_grh_h.bin",
};

static const char *LOG_SERIES_NAMES[SENSOR_LOG_COUNT] = {"inc", "grh_t", "grh_h"};
static const char *LOG_SERIES_LABELS[SENSOR_LOG_COUNT] = {
    "INC temperature",
    "GRH temperature",
    "GRH humidity",
};

#ifndef ESP_PARTITION_SUBTYPE_DATA_LITTLEFS
#define ESP_PARTITION_SUBTYPE_DATA_LITTLEFS 0x83
#endif
#ifndef ESP_PARTITION_SUBTYPE_DATA_FAT
#define ESP_PARTITION_SUBTYPE_DATA_FAT 0x81
#endif

#pragma pack(push, 1)
struct SensorLogFileHeader {
  uint32_t magic;
  uint8_t version;
  uint8_t series;
  uint16_t reserved;
  uint32_t capacity;
  uint32_t count;
  uint32_t head;
};

struct SensorLogRecord {
  uint32_t ts;
  int16_t value;
};
#pragma pack(pop)

static const uint32_t LOG_MAGIC = 0x504C4753UL; // 'PLGS'
static const uint8_t LOG_VERSION = 1;

static Preferences s_logPrefs;
static bool s_prefsReady = false;
static bool s_fsReady = false;
static fs::FS *s_logFs = nullptr;
static const char *s_logFsKind = nullptr;
static bool s_enabled = true;
static uint8_t s_intervalMin = SENSOR_LOG_INTERVAL_MIN;
static unsigned long s_lastLogMs = 0;

static char s_logConfigJsonBuf[384];
static char s_logHistoryJsonBuf[SENSOR_LOG_HISTORY_JSON_CAP];

static File sensor_log_openFile(const char *path, const char *mode) {
  return s_logFs != nullptr ? s_logFs->open(path, mode) : File();
}

static bool sensor_log_removeFile(const char *path) {
  return s_logFs != nullptr && s_logFs->remove(path);
}

static void sensor_log_printMountHint() {
  app_log(F("Sensor log: storage unavailable"));
  app_log(F("  Expected LittleFS (spiffs) or FFat (ffat) data partition"));
  app_log(F("  Nano ESP32 default scheme uses label=ffat — should mount FFat"));

  esp_partition_iterator_t it =
      esp_partition_find(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY, nullptr);
  bool anyFs = false;
  while (it != nullptr) {
    const esp_partition_t *part = esp_partition_get(it);
    const uint8_t subtype = part->subtype;
    if (subtype == ESP_PARTITION_SUBTYPE_DATA_SPIFFS ||
        subtype == ESP_PARTITION_SUBTYPE_DATA_LITTLEFS ||
        subtype == ESP_PARTITION_SUBTYPE_DATA_FAT) {
      anyFs = true;
      Serial.print(F("  FS partition: label="));
      Serial.print(part->label);
      Serial.print(F(" size="));
      Serial.println((unsigned)part->size);
    }
    it = esp_partition_next(it);
  }
  esp_partition_iterator_release(it);
  if (!anyFs) {
    app_log(F("  No SPIFFS/LittleFS/FAT data partition in current flash layout"));
  }
}

static bool sensor_log_tryMountLittleFs(const char *label, bool formatOnFail) {
  if (label != nullptr && label[0] != '\0') {
    return LittleFS.begin(formatOnFail, "/littlefs", 4, label);
  }
  return LittleFS.begin(formatOnFail);
}

static bool sensor_log_tryMountFFat(const char *label, bool formatOnFail) {
  if (label != nullptr && label[0] != '\0') {
    return FFat.begin(formatOnFail, "/ffat", 4, label);
  }
  return FFat.begin(formatOnFail);
}

static bool sensor_log_mountFs(char *labelOut, size_t labelOutLen) {
  static const char *kLittleLabels[] = {"spiffs", "littlefs", "storage", nullptr};
  static const char *kFatLabels[] = {"ffat", "fat", nullptr};
  const bool formatOnFail = false;

  for (uint8_t i = 0; kLittleLabels[i] != nullptr; i++) {
    if (sensor_log_tryMountLittleFs(kLittleLabels[i], formatOnFail)) {
      s_logFs = &LittleFS;
      s_logFsKind = "LittleFS";
      if (labelOut != nullptr && labelOutLen > 0) {
        snprintf(labelOut, labelOutLen, "%s", kLittleLabels[i]);
      }
      return true;
    }
  }
  if (sensor_log_tryMountLittleFs(nullptr, formatOnFail)) {
    s_logFs = &LittleFS;
    s_logFsKind = "LittleFS";
    if (labelOut != nullptr && labelOutLen > 0) {
      snprintf(labelOut, labelOutLen, "default");
    }
    return true;
  }
  for (uint8_t i = 0; kFatLabels[i] != nullptr; i++) {
    if (sensor_log_tryMountFFat(kFatLabels[i], formatOnFail)) {
      s_logFs = &FFat;
      s_logFsKind = "FFat";
      if (labelOut != nullptr && labelOutLen > 0) {
        snprintf(labelOut, labelOutLen, "%s", kFatLabels[i]);
      }
      return true;
    }
  }
  if (sensor_log_tryMountFFat(nullptr, formatOnFail)) {
    s_logFs = &FFat;
    s_logFsKind = "FFat";
    if (labelOut != nullptr && labelOutLen > 0) {
      snprintf(labelOut, labelOutLen, "default");
    }
    return true;
  }
  s_logFs = nullptr;
  s_logFsKind = nullptr;
  return false;
}

static uint8_t sensor_log_clampInterval(uint8_t minutes) {
  if (minutes < SENSOR_LOG_INTERVAL_MIN) {
    minutes = SENSOR_LOG_INTERVAL_MIN;
  }
  if (minutes > SENSOR_LOG_INTERVAL_MAX) {
    minutes = SENSOR_LOG_INTERVAL_MAX;
  }
  if (minutes <= 7) {
    return 5;
  }
  if (minutes <= 12) {
    return 10;
  }
  return 15;
}

static void sensor_log_savePrefs() {
  if (!s_prefsReady) {
    return;
  }
  s_logPrefs.putUChar(LOG_KEY_ENABLED, s_enabled ? 1 : 0);
  s_logPrefs.putUChar(LOG_KEY_INTERVAL, s_intervalMin);
}

static void sensor_log_loadPrefs() {
  s_enabled = true;
  s_intervalMin = SENSOR_LOG_INTERVAL_MIN;
  if (!s_prefsReady) {
    return;
  }
  if (s_logPrefs.isKey(LOG_KEY_ENABLED)) {
    s_enabled = s_logPrefs.getUChar(LOG_KEY_ENABLED, 1) != 0;
  }
  if (s_logPrefs.isKey(LOG_KEY_INTERVAL)) {
    s_intervalMin = sensor_log_clampInterval(s_logPrefs.getUChar(LOG_KEY_INTERVAL, SENSOR_LOG_INTERVAL_MIN));
  }
}

static bool sensor_log_headerValid(const SensorLogFileHeader &hdr, SensorLogSeries series) {
  if (hdr.magic != LOG_MAGIC || hdr.version != LOG_VERSION || hdr.series != (uint8_t)series) {
    return false;
  }
  if (hdr.capacity != SENSOR_LOG_CAPACITY || hdr.capacity == 0) {
    return false;
  }
  if (hdr.head >= hdr.capacity || hdr.count > hdr.capacity) {
    return false;
  }
  // While the ring is not full, head == count (sequential write).
  if (hdr.count < hdr.capacity && hdr.head != hdr.count) {
    return false;
  }
  return true;
}

static bool sensor_log_initFile(SensorLogSeries series) {
  if (!s_fsReady || series >= SENSOR_LOG_COUNT) {
    return false;
  }

  File f = sensor_log_openFile(LOG_FILE_PATHS[series], "r");
  if (f) {
    SensorLogFileHeader hdr = {};
    const size_t n = f.read((uint8_t *)&hdr, sizeof(hdr));
    f.close();
    if (n == sizeof(hdr) && sensor_log_headerValid(hdr, series)) {
      return true;
    }
  }

  f = sensor_log_openFile(LOG_FILE_PATHS[series], "w");
  if (!f) {
    return false;
  }
  SensorLogFileHeader hdr = {};
  hdr.magic = LOG_MAGIC;
  hdr.version = LOG_VERSION;
  hdr.series = (uint8_t)series;
  hdr.capacity = SENSOR_LOG_CAPACITY;
  hdr.count = 0;
  hdr.head = 0;
  const size_t written = f.write((const uint8_t *)&hdr, sizeof(hdr));
  f.close();
  return written == sizeof(hdr);
}

static bool sensor_log_readHeader(SensorLogSeries series, SensorLogFileHeader &hdr) {
  if (!s_fsReady || series >= SENSOR_LOG_COUNT) {
    return false;
  }
  File f = sensor_log_openFile(LOG_FILE_PATHS[series], "r");
  if (!f) {
    return false;
  }
  const size_t n = f.read((uint8_t *)&hdr, sizeof(hdr));
  f.close();
  return n == sizeof(hdr) && sensor_log_headerValid(hdr, series);
}

static bool sensor_log_readHeaderFromFile(File &f, SensorLogSeries series, SensorLogFileHeader &hdr) {
  if (!f || !f.seek(0)) {
    return false;
  }
  const size_t n = f.read((uint8_t *)&hdr, sizeof(hdr));
  return n == sizeof(hdr) && sensor_log_headerValid(hdr, series);
}

static bool sensor_log_openSeriesFile(SensorLogSeries series, const char *mode, File &out) {
  out = sensor_log_openFile(LOG_FILE_PATHS[series], mode);
  if (out) {
    return true;
  }
  if (!sensor_log_initFile(series)) {
    return false;
  }
  out = sensor_log_openFile(LOG_FILE_PATHS[series], mode);
  return static_cast<bool>(out);
}

static bool sensor_log_logicalToPhysIdx(const SensorLogFileHeader &hdr, uint32_t logicalIdx, uint32_t &physIdxOut) {
  if (logicalIdx >= hdr.count) {
    return false;
  }
  if (hdr.count < hdr.capacity) {
    physIdxOut = logicalIdx;
  } else {
    physIdxOut = (hdr.head + logicalIdx) % hdr.capacity;
  }
  return true;
}

static bool sensor_log_readRecordAt(File &f, const SensorLogFileHeader &hdr, uint32_t logicalIdx,
                                    SensorLogRecord &out) {
  uint32_t physIdx = 0;
  if (!sensor_log_logicalToPhysIdx(hdr, logicalIdx, physIdx)) {
    return false;
  }
  const size_t offset = sizeof(SensorLogFileHeader) + physIdx * sizeof(SensorLogRecord);
  if (!f.seek(offset) || f.read((uint8_t *)&out, sizeof(out)) != sizeof(out)) {
    return false;
  }
  return out.ts != 0;
}

// Write record and header in one file session; flush narrows the loss window on power failure.
static bool sensor_log_appendRecord(SensorLogSeries series, uint32_t ts, int16_t value) {
  if (!s_fsReady || series >= SENSOR_LOG_COUNT) {
    return false;
  }

  File f;
  if (!sensor_log_openSeriesFile(series, "r+", f)) {
    return false;
  }

  SensorLogFileHeader hdr = {};
  if (!sensor_log_readHeaderFromFile(f, series, hdr)) {
    f.close();
    if (!sensor_log_initFile(series) || !sensor_log_openSeriesFile(series, "r+", f)) {
      return false;
    }
    if (!sensor_log_readHeaderFromFile(f, series, hdr)) {
      f.close();
      return false;
    }
  }

  const uint32_t idx = hdr.head;
  const size_t recOffset = sizeof(SensorLogFileHeader) + idx * sizeof(SensorLogRecord);
  if (!f.seek(recOffset)) {
    f.close();
    return false;
  }
  const SensorLogRecord rec = {ts, value};
  if (f.write((const uint8_t *)&rec, sizeof(rec)) != sizeof(rec)) {
    f.close();
    return false;
  }

  hdr.head = (hdr.head + 1) % hdr.capacity;
  if (hdr.count < hdr.capacity) {
    hdr.count++;
  }

  if (!f.seek(0) || f.write((const uint8_t *)&hdr, sizeof(hdr)) != sizeof(hdr)) {
    f.close();
    return false;
  }
  f.flush();
  f.close();
  return true;
}

bool sensor_log_isCompiledIn() {
  return true;
}

bool sensor_log_isReady() {
  return s_fsReady;
}

bool sensor_log_isEnabled() {
  return s_enabled;
}

void sensor_log_setEnabled(bool enabled) {
  s_enabled = enabled;
  sensor_log_savePrefs();
}

uint8_t sensor_log_getIntervalMin() {
  return s_intervalMin;
}

bool sensor_log_setIntervalMin(uint8_t minutes) {
  const uint8_t clamped = sensor_log_clampInterval(minutes);
  if (clamped == s_intervalMin) {
    return true;
  }
  s_intervalMin = clamped;
  s_lastLogMs = 0;
  sensor_log_savePrefs();
  return true;
}

uint32_t sensor_log_recordCount(SensorLogSeries series) {
  SensorLogFileHeader hdr = {};
  if (!sensor_log_readHeader(series, hdr)) {
    return 0;
  }
  return hdr.count;
}

const char *sensor_log_seriesName(SensorLogSeries series) {
  return series < SENSOR_LOG_COUNT ? LOG_SERIES_NAMES[series] : "";
}

const char *sensor_log_seriesLabel(SensorLogSeries series) {
  return series < SENSOR_LOG_COUNT ? LOG_SERIES_LABELS[series] : "";
}

bool sensor_log_parseSeries(const char *name, SensorLogSeries &out) {
  if (name == nullptr) {
    return false;
  }
  for (uint8_t i = 0; i < SENSOR_LOG_COUNT; i++) {
    if (strcmp(name, LOG_SERIES_NAMES[i]) == 0) {
      out = (SensorLogSeries)i;
      return true;
    }
  }
  return false;
}

void sensor_log_begin() {
  s_prefsReady = s_logPrefs.begin(LOG_NVS_NS, false);
  sensor_log_loadPrefs();

  char mountLabel[12] = {};
  s_fsReady = sensor_log_mountFs(mountLabel, sizeof(mountLabel));
  if (s_fsReady) {
    Serial.print(F("Sensor log: "));
    if (s_logFsKind != nullptr) {
      Serial.print(s_logFsKind);
    } else {
      Serial.print(F("FS"));
    }
    Serial.print(F(" OK ("));
    Serial.print(mountLabel);
    Serial.println(F(")"));
  } else {
    sensor_log_printMountHint();
  }
  s_lastLogMs = 0;
}

void sensor_log_poll(unsigned long nowMillis) {
  if (!s_fsReady || !s_enabled) {
    return;
  }
  const unsigned long intervalMs = (unsigned long)s_intervalMin * 60000UL;
  if (!millisIntervalElapsed(nowMillis, s_lastLogMs, intervalMs)) {
    return;
  }
  s_lastLogMs = nowMillis;

  if (!rtc_hasValidNow()) {
    return;
  }

  AppSnapshot snap;
  app_buildSnapshot(snap);
  const uint32_t ts = rtc_getNow().unixtime();

  if (snap.incTemp.valid) {
    sensor_log_appendRecord(SENSOR_LOG_INC, ts, snap.incTemp.value);
  }
  if (snap.grhTemp.valid) {
    sensor_log_appendRecord(SENSOR_LOG_GRH_T, ts, snap.grhTemp.value);
  }
  if (snap.grhHum.valid) {
    sensor_log_appendRecord(SENSOR_LOG_GRH_H, ts, snap.grhHum.value);
  }
}

bool sensor_log_clear() {
  if (!s_fsReady) {
    return false;
  }
  for (uint8_t i = 0; i < SENSOR_LOG_COUNT; i++) {
    sensor_log_removeFile(LOG_FILE_PATHS[i]);
    if (!sensor_log_initFile((SensorLogSeries)i)) {
      return false;
    }
  }
  s_lastLogMs = 0;
  return true;
}

const char *sensor_log_getConfigJson() {
  int off = wifi_json_off(s_logConfigJsonBuf, sizeof(s_logConfigJsonBuf), 0, "{\"compiled_in\":true,\"ready\":%s",
                          s_fsReady ? "true" : "false");
  off = wifi_json_off(s_logConfigJsonBuf, sizeof(s_logConfigJsonBuf), off, ",\"enabled\":%s",
                      s_enabled ? "true" : "false");
  off = wifi_json_off(s_logConfigJsonBuf, sizeof(s_logConfigJsonBuf), off, ",\"interval_min\":%u",
                      (unsigned)s_intervalMin);
  off = wifi_json_append_cstr(s_logConfigJsonBuf, sizeof(s_logConfigJsonBuf), off,
                              ",\"interval_min_allowed\":[5,10,15],\"retention_days\":");
  off = wifi_json_off(s_logConfigJsonBuf, sizeof(s_logConfigJsonBuf), off, "%u,\"capacity\":%u,\"records\":{",
                      (unsigned)SENSOR_LOG_RETENTION_DAYS, (unsigned)SENSOR_LOG_CAPACITY);
  for (uint8_t i = 0; i < SENSOR_LOG_COUNT; i++) {
    if (i > 0) {
      off = wifi_json_append_cstr(s_logConfigJsonBuf, sizeof(s_logConfigJsonBuf), off, ",");
    }
    off = wifi_json_off(s_logConfigJsonBuf, sizeof(s_logConfigJsonBuf), off, "\"%s\":%lu", LOG_SERIES_NAMES[i],
                        (unsigned long)sensor_log_recordCount((SensorLogSeries)i));
  }
  off = wifi_json_append_cstr(s_logConfigJsonBuf, sizeof(s_logConfigJsonBuf), off, "}}");
  if (!wifi_json_valid(off, sizeof(s_logConfigJsonBuf))) {
    return wifi_json_overflow_response();
  }
  return s_logConfigJsonBuf;
}

const char *sensor_log_getHistoryJson(SensorLogSeries series, uint32_t hours, uint16_t maxPoints) {
  if (!s_fsReady || series >= SENSOR_LOG_COUNT) {
    return "{\"ok\":false,\"error\":\"storage\"}";
  }
  if (!s_enabled) {
    return "{\"ok\":false,\"error\":\"disabled\"}";
  }
  if (!rtc_hasValidNow()) {
    return "{\"ok\":false,\"error\":\"rtc\"}";
  }

  if (hours == 0) {
    hours = 24;
  }
  if (hours > (uint32_t)SENSOR_LOG_RETENTION_DAYS * 24U) {
    hours = (uint32_t)SENSOR_LOG_RETENTION_DAYS * 24U;
  }
  if (maxPoints == 0) {
    maxPoints = SENSOR_LOG_HISTORY_DEFAULT_POINTS;
  }
  if (maxPoints > SENSOR_LOG_HISTORY_MAX_POINTS) {
    maxPoints = SENSOR_LOG_HISTORY_MAX_POINTS;
  }

  const uint32_t nowTs = rtc_getNow().unixtime();
  const uint32_t fromTs = (nowTs > hours * 3600UL) ? (nowTs - hours * 3600UL) : 0;

  File f;
  if (!sensor_log_openSeriesFile(series, "r", f)) {
    return "{\"ok\":false,\"error\":\"storage\"}";
  }

  SensorLogFileHeader hdr = {};
  if (!sensor_log_readHeaderFromFile(f, series, hdr) || hdr.count == 0) {
    f.close();
    int off = wifi_json_off(s_logHistoryJsonBuf, sizeof(s_logHistoryJsonBuf), 0,
                            "{\"ok\":true,\"series\":\"%s\",\"label\":\"", LOG_SERIES_NAMES[series]);
    off = wifi_json_append_escape(s_logHistoryJsonBuf, sizeof(s_logHistoryJsonBuf), off, LOG_SERIES_LABELS[series]);
    off = wifi_json_off(s_logHistoryJsonBuf, sizeof(s_logHistoryJsonBuf), off, "\",\"unit\":\"%s\",\"points\":[]}",
                        (series == SENSOR_LOG_GRH_H) ? "%" : "C");
    if (!wifi_json_valid(off, sizeof(s_logHistoryJsonBuf))) {
      return wifi_json_overflow_response();
    }
    return s_logHistoryJsonBuf;
  }

  uint32_t matchCount = 0;
  for (uint32_t i = 0; i < hdr.count; i++) {
    if ((i & 0xFFU) == 0) {
      app_watchdogReset();
    }
    SensorLogRecord rec = {};
    if (!sensor_log_readRecordAt(f, hdr, i, rec)) {
      continue;
    }
    if (rec.ts >= fromTs && rec.ts <= nowTs) {
      matchCount++;
    }
  }

  int off = wifi_json_off(s_logHistoryJsonBuf, sizeof(s_logHistoryJsonBuf), 0,
                          "{\"ok\":true,\"series\":\"%s\",\"label\":\"", LOG_SERIES_NAMES[series]);
  off = wifi_json_append_escape(s_logHistoryJsonBuf, sizeof(s_logHistoryJsonBuf), off, LOG_SERIES_LABELS[series]);
  off = wifi_json_off(s_logHistoryJsonBuf, sizeof(s_logHistoryJsonBuf), off,
                      "\",\"unit\":\"%s\",\"from\":%lu,\"to\":%lu,\"points\":[",
                      (series == SENSOR_LOG_GRH_H) ? "%" : "C", (unsigned long)fromTs, (unsigned long)nowTs);
  if (off < 0) {
    f.close();
    return wifi_json_overflow_response();
  }

  if (matchCount == 0) {
    f.close();
    off = wifi_json_append_cstr(s_logHistoryJsonBuf, sizeof(s_logHistoryJsonBuf), off, "]}");
    if (!wifi_json_valid(off, sizeof(s_logHistoryJsonBuf))) {
      return wifi_json_overflow_response();
    }
    return s_logHistoryJsonBuf;
  }

  uint32_t step = 1;
  if (matchCount > maxPoints) {
    step = (matchCount + maxPoints - 1) / maxPoints;
    if (step == 0) {
      step = 1;
    }
  }

  bool first = true;
  uint32_t seen = 0;
  uint16_t emitted = 0;
  for (uint32_t i = 0; i < hdr.count && emitted < maxPoints; i++) {
    if ((i & 0xFFU) == 0) {
      app_watchdogReset();
    }
    SensorLogRecord rec = {};
    if (!sensor_log_readRecordAt(f, hdr, i, rec)) {
      continue;
    }
    if (rec.ts < fromTs || rec.ts > nowTs) {
      continue;
    }
    if (seen % step != 0) {
      seen++;
      continue;
    }
    seen++;
    emitted++;
    if (!first) {
      off = wifi_json_append_cstr(s_logHistoryJsonBuf, sizeof(s_logHistoryJsonBuf), off, ",");
    } else {
      first = false;
    }
    if (series == SENSOR_LOG_GRH_H) {
      off = wifi_json_off(s_logHistoryJsonBuf, sizeof(s_logHistoryJsonBuf), off, "[%lu,%d]", (unsigned long)rec.ts,
                          (int)rec.value);
    } else {
      off = wifi_json_off(s_logHistoryJsonBuf, sizeof(s_logHistoryJsonBuf), off, "[%lu,%.1f]", (unsigned long)rec.ts,
                          (double)rec.value / 10.0);
    }
    if (off < 0) {
      f.close();
      return wifi_json_overflow_response();
    }
  }
  f.close();
  off = wifi_json_append_cstr(s_logHistoryJsonBuf, sizeof(s_logHistoryJsonBuf), off, "]}");
  if (!wifi_json_valid(off, sizeof(s_logHistoryJsonBuf))) {
    return wifi_json_overflow_response();
  }
  return s_logHistoryJsonBuf;
}

#endif // SENSOR_LOG_ENABLE && ARDUINO_ARCH_ESP32 && !SIM_BUILD
