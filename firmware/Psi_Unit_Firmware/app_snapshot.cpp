/*
 * app_snapshot.cpp — build state snapshot and main-screen stale flag
 *
 * app_buildSnapshot() reads current data from sensors, logic, and rtc modules.
 * app_markMainScreenDirty() signals that main-screen values have changed.
 */

#include "app_snapshot.h"
#include "config.h"
#include "storage.h"
#include "sensors.h"
#include "logic.h"
#include "rtc_clock.h"

static bool s_mainScreenDirty = false;

void app_markMainScreenDirty() {
  s_mainScreenDirty = true;
}

bool app_consumeMainScreenDirty() {
  if (!s_mainScreenDirty) {
    return false;
  }
  s_mainScreenDirty = false;
  return true;
}

// Build current snapshot from sensors, logic, rtc, and storage modules.
void app_buildSnapshot(AppSnapshot &out) {
  out.nvsReady = storage_isReady();

  out.rtc.ok = !rtcError && rtc_hasValidNow();
  if (out.rtc.ok) {
    const DateTime &now = rtc_getNow();
    out.rtc.hour = now.hour();
    out.rtc.minute = now.minute();
    out.rtc.second = now.second();
    out.rtc.day = now.day();
    out.rtc.month = now.month();
    out.rtc.year = now.year();
  }

  out.incTemp.valid = ds18State.dataValid && sensor_hasValue(tDS);
  out.incTemp.value = tDS;
  out.grhTemp.valid = shtTempState.dataValid && sensor_hasValue(tSHT);
  out.grhTemp.value = tSHT;
  out.grhHum.valid = shtHumState.dataValid && sensor_hasValue(hSHT);
  out.grhHum.value = hSHT;

  out.outputs.heatOn = heatOn;
  out.outputs.humOn = humOn;
  out.outputs.fanOn = fanOn;
  out.outputs.lightOn = lightOn;
  out.outputs.incOn = incOn;
  out.outputs.heatMode = (Mode)settings.heatMode;
  out.outputs.humMode = (Mode)settings.humMode;
  out.outputs.fanMode = (Mode)settings.fanMode;
  out.outputs.lightMode = (Mode)settings.lightMode;
  out.outputs.incMode = (Mode)settings.incMode;
  out.outputs.lightOverride = logic_isLightOverrideActive();
}
