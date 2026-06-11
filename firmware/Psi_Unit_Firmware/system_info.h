/*
 * system_info.h — Sys Info screen: controller diagnostics
 *
 * Row table (uptime, version, Wi-Fi, I2C, memory) for OLED and web. Refreshed
 * every second without redundant I2C access.
 */

#ifndef SYSTEM_INFO_H
#define SYSTEM_INFO_H

#include <Arduino.h>

enum SystemInfoRowKind : uint8_t {
  SYSINFO_ROW_DIVIDER = 0,
  SYSINFO_ROW_SECTION,
  SYSINFO_ROW_FIELD
};

#ifndef SYSINFO_VALUE_CHARS
#define SYSINFO_VALUE_CHARS 12
#endif

#ifndef SYSINFO_MAX_ROWS
#define SYSINFO_MAX_ROWS 24
#endif

void system_info_init();
bool system_info_tick(unsigned long nowMillis);
void system_info_refresh();
uint8_t system_info_rowCount();
SystemInfoRowKind system_info_rowKind(uint8_t index);
const char *system_info_rowLabel(uint8_t index);
const char *system_info_rowValue(uint8_t index);
void system_info_formatUptimeSec(uint32_t sec, char *dst, size_t dstLen);

#endif // SYSTEM_INFO_H
