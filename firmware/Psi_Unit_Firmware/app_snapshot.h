/*
 * app_snapshot.h — current state snapshot for display and web interface
 *
 * Collects sensor readings, RTC time, output state, and NVS readiness into one
 * structure. UI and API read only the snapshot, not globals directly.
 */

#ifndef APP_SNAPSHOT_H
#define APP_SNAPSHOT_H

#include "types.h"

struct AppRtcSnapshot {
  bool ok;
  uint8_t hour;
  uint8_t minute;
  uint8_t second;
  uint8_t day;
  uint8_t month;
  uint16_t year;
};

struct AppSensorSnapshot {
  int16_t value;
  bool valid;
};

struct AppOutputsSnapshot {
  bool heatOn;
  bool humOn;
  bool fanOn;
  bool lightOn;
  bool incOn;
  Mode heatMode;
  Mode humMode;
  Mode fanMode;
  Mode lightMode;
  Mode incMode;
  bool lightOverride;
};

struct AppSnapshot {
  bool nvsReady;
  AppRtcSnapshot rtc;
  AppSensorSnapshot incTemp;
  AppSensorSnapshot grhTemp;
  AppSensorSnapshot grhHum;
  AppOutputsSnapshot outputs;
};

void app_buildSnapshot(AppSnapshot &out);
void app_markMainScreenDirty();
bool app_consumeMainScreenDirty();

#endif // APP_SNAPSHOT_H
