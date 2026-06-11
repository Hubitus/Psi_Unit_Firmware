/*
 * sim_system_info.cpp — System Info screen without ESP32 API
 */

#include "system_info.h"
#include "config.h"
#include "rtc_clock.h"
#include "i2c_bus.h"
#include <stdio.h>
#include <string.h>

static SystemInfoRowKind s_kind[SYSINFO_MAX_ROWS];
static char s_label[SYSINFO_MAX_ROWS][8];
static char s_value[SYSINFO_MAX_ROWS][SYSINFO_VALUE_CHARS + 1];
static uint8_t s_rowCount = 0;
static unsigned long s_lastPollMs = 0;
static uint32_t s_uptimeSec = 0;
static char s_buildStamp[20];

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

static void sysinfo_refreshRows() {
  uint8_t n = 0;
  char val[SYSINFO_VALUE_CHARS + 1];

  sysinfo_section(n, "System");
  sysinfo_field(n, "Ver", FIRMWARE_VERSION);
  sysinfo_field(n, "Bld", s_buildStamp);
  system_info_formatUptimeSec(s_uptimeSec, val, sizeof(val));
  sysinfo_field(n, "Up", val);
  sysinfo_field(n, "CPU", "desktop");
  sysinfo_field(n, "Rst", "sim");

  sysinfo_section(n, "Clock");
  if (rtc_hasValidNow()) {
    const DateTime &now = rtc_getNow();
    snprintf(val, sizeof(val), "%02u:%02u %02u/%02u/%02u", now.hour(), now.minute(), now.day(), now.month(),
             now.year() % 100U);
  } else {
    strncpy(val, "--", sizeof(val));
  }
  sysinfo_field(n, "Time", val);
  sysinfo_field(n, "RTC", rtcError ? "Error" : "OK");

  sysinfo_section(n, "Memory");
  sysinfo_field(n, "Heap", "n/a");
  sysinfo_field(n, "Min", "n/a");
  sysinfo_field(n, "App", "n/a");
  sysinfo_field(n, "Free", "n/a");

  sysinfo_section(n, "I/O");
  snprintf(val, sizeof(val), "%s RTC:%c", i2c_isHealthy() ? "OK" : "ERR", i2c_isRtcPresent() ? 'Y' : 'N');
  sysinfo_field(n, "I2C", val);
  sysinfo_field(n, "WiFi", "off");

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
