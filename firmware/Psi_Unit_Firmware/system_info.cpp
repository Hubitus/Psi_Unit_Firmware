/*
 * system_info.cpp — collect and cache diagnostic strings for the Sys Info menu
 *
 * Refreshed at most once per second; RTC uses rtcNowCached without calling
 * rtc.now() on every tick.
 */

#include "system_info.h"
#include "wifi_manager.h"
#include "config.h"
#include "rtc_clock.h"
#include "i2c_bus.h"
#include <stdio.h>
#include <string.h>
#include <esp_system.h>

#if !defined(ARDUINO_ARCH_ESP32)
#error "system_info.cpp: Arduino Nano ESP32 only."
#endif

static SystemInfoRowKind s_kind[SYSINFO_MAX_ROWS];
static char s_label[SYSINFO_MAX_ROWS][8];
static char s_value[SYSINFO_MAX_ROWS][SYSINFO_VALUE_CHARS + 1];
static uint8_t s_rowCount = 0;
static unsigned long s_lastPollMs = 0;
static uint32_t s_uptimeSec = 0;
static char s_buildStamp[20];

static void sysinfo_kb(char *dst, size_t dstLen, uint32_t bytes) {
  snprintf(dst, dstLen, "%lu KB", (unsigned long)((bytes + 511UL) / 1024UL));
}

static void sysinfo_divider(uint8_t &n) {
  if (n >= SYSINFO_MAX_ROWS) {
    return;
  }
  s_kind[n] = SYSINFO_ROW_DIVIDER;
  s_label[n][0] = '\0';
  s_value[n][0] = '\0';
  n++;
}

static void sysinfo_section(uint8_t &n, const char *title) {
  if (n >= SYSINFO_MAX_ROWS) {
    return;
  }
  s_kind[n] = SYSINFO_ROW_SECTION;
  snprintf(s_label[n], sizeof(s_label[n]), "%s", title);
  s_value[n][0] = '\0';
  n++;
}

static void sysinfo_field(uint8_t &n, const char *lbl, const char *val) {
  if (n >= SYSINFO_MAX_ROWS) {
    return;
  }
  s_kind[n] = SYSINFO_ROW_FIELD;
  strncpy(s_label[n], lbl, sizeof(s_label[n]) - 1);
  s_label[n][sizeof(s_label[n]) - 1] = '\0';
  strncpy(s_value[n], val, SYSINFO_VALUE_CHARS);
  s_value[n][SYSINFO_VALUE_CHARS] = '\0';
  n++;
}

static void sysinfo_resetReason(char *dst, size_t dstLen) {
  const char *tag = "?";
  switch (esp_reset_reason()) {
    case ESP_RST_POWERON: tag = "PwrOn"; break;
    case ESP_RST_EXT: tag = "Ext"; break;
    case ESP_RST_SW: tag = "SW"; break;
    case ESP_RST_PANIC: tag = "Panic"; break;
    case ESP_RST_INT_WDT: tag = "IntWDT"; break;
    case ESP_RST_TASK_WDT: tag = "TskWDT"; break;
    case ESP_RST_WDT: tag = "WDT"; break;
    case ESP_RST_DEEPSLEEP: tag = "Sleep"; break;
    case ESP_RST_BROWNOUT: tag = "Brown"; break;
    case ESP_RST_SDIO: tag = "SDIO"; break;
    default: break;
  }
  strncpy(dst, tag, dstLen);
  dst[dstLen - 1] = '\0';
}

void system_info_formatUptimeSec(uint32_t sec, char *dst, size_t dstLen) {
  if (dst == nullptr || dstLen == 0U) {
    return;
  }
  const uint32_t days = sec / 86400UL;
  const uint32_t remDay = sec % 86400UL;
  const uint32_t hours = remDay / 3600UL;
  const uint32_t remHour = remDay % 3600UL;
  const uint32_t mins = remHour / 60UL;
  const uint32_t secs = remHour % 60UL;
  if (days > 0U) {
    snprintf(dst, dstLen, "%lud %uh", (unsigned long)days, (unsigned)hours);
  } else if (hours > 0U) {
    snprintf(dst, dstLen, "%uh %um", (unsigned)hours, (unsigned)mins);
  } else if (mins > 0U) {
    snprintf(dst, dstLen, "%um %us", (unsigned)mins, (unsigned)secs);
  } else {
    snprintf(dst, dstLen, "%us", (unsigned)secs);
  }
  dst[dstLen - 1] = '\0';
}

static void sysinfo_formatRtcTime(char *dst, size_t dstLen) {
  if (rtcError || !rtc_hasValidNow()) {
    strncpy(dst, "--", dstLen);
    return;
  }
  const DateTime &now = rtc_getNow();
  snprintf(dst, dstLen, "%02u:%02u %02u/%02u/%02u",
           now.hour(), now.minute(),
           now.day(), now.month(), now.year() % 100U);
}

static void sysinfo_formatRtcStatus(char *dst, size_t dstLen) {
  if (!i2c_isRtcPresent()) {
    strncpy(dst, "No chip", dstLen);
  } else if (rtcError) {
    strncpy(dst, "Error", dstLen);
  } else if (!rtc_hasValidNow()) {
    strncpy(dst, "Invalid", dstLen);
  } else {
    strncpy(dst, "OK", dstLen);
  }
  dst[dstLen - 1] = '\0';
}

static void sysinfo_formatI2c(char *dst, size_t dstLen) {
  const bool rtcHw = i2c_isRtcPresent();
  const bool busOk = i2c_isHealthy();
  snprintf(dst, dstLen, "%s RTC:%c",
           busOk ? "OK" : "ERR",
           rtcHw ? 'Y' : 'N');
}

static void sysinfo_refreshRows() {
  uint8_t n = 0;
  char val[SYSINFO_VALUE_CHARS + 1];

  sysinfo_section(n, "System");
  sysinfo_field(n, "Ver", FIRMWARE_VERSION);
  sysinfo_field(n, "Bld", s_buildStamp);
  system_info_formatUptimeSec(s_uptimeSec, val, sizeof(val));
  sysinfo_field(n, "Up", val);
  snprintf(val, sizeof(val), "%u MHz", (unsigned)ESP.getCpuFreqMHz());
  sysinfo_field(n, "CPU", val);
  sysinfo_resetReason(val, sizeof(val));
  sysinfo_field(n, "Rst", val);

  sysinfo_section(n, "Clock");
  sysinfo_formatRtcTime(val, sizeof(val));
  sysinfo_field(n, "Time", val);
  sysinfo_formatRtcStatus(val, sizeof(val));
  sysinfo_field(n, "RTC", val);

  sysinfo_section(n, "Memory");
  sysinfo_kb(val, sizeof(val), ESP.getFreeHeap());
  sysinfo_field(n, "Heap", val);
  sysinfo_kb(val, sizeof(val), ESP.getMinFreeHeap());
  sysinfo_field(n, "Min", val);
  sysinfo_kb(val, sizeof(val), ESP.getSketchSize());
  sysinfo_field(n, "App", val);
  sysinfo_kb(val, sizeof(val), ESP.getFreeSketchSpace());
  sysinfo_field(n, "Free", val);

  sysinfo_section(n, "I/O");
  sysinfo_formatI2c(val, sizeof(val));
  sysinfo_field(n, "I2C", val);
  wifi_manager_getStatusText(val, sizeof(val));
  sysinfo_field(n, "WiFi", val);

  s_rowCount = n;
}

void system_info_init() {
  snprintf(s_buildStamp, sizeof(s_buildStamp), "%.11s", __DATE__);
  s_uptimeSec = 0;
  s_lastPollMs = millis();
  sysinfo_refreshRows();
}

bool system_info_tick(unsigned long nowMillis) {
  if (!millisIntervalElapsed(nowMillis, s_lastPollMs, SYSINFO_UPDATE_INTERVAL_MS)) {
    return false;
  }
  s_lastPollMs = nowMillis;
  s_uptimeSec++;
  return true;
}

void system_info_refresh() {
  sysinfo_refreshRows();
}

uint8_t system_info_rowCount() {
  return s_rowCount;
}

SystemInfoRowKind system_info_rowKind(uint8_t index) {
  if (index >= s_rowCount) {
    return SYSINFO_ROW_FIELD;
  }
  return s_kind[index];
}

const char *system_info_rowLabel(uint8_t index) {
  if (index >= s_rowCount) {
    return "";
  }
  return s_label[index];
}

const char *system_info_rowValue(uint8_t index) {
  if (index >= s_rowCount) {
    return "";
  }
  return s_value[index];
}
