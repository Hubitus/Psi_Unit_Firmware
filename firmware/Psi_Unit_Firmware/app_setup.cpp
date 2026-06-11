/*
 * app_setup.cpp — controller startup on power-on
 *
 * Prepares the device: I2C bus, sensors, display, RTC, loads persisted settings,
 * starts Wi‑Fi and sensor log. Called once from setup() in the main firmware file.
 */

#include "app_setup.h"
#include "app_platform.h"
#include "app_state.h"
#include "config.h"
#include "storage.h"
#include "app_effects.h"
#include "sensors.h"
#include "rtc_clock.h"
#include "logic.h"
#include "display_ui.h"
#include "display_backend.h"
#include "i2c_bus.h"
#include "rgb_status.h"
#include "system_info.h"
#include "input_backend.h"
#include "wifi_manager.h"
#include "sensor_log.h"

// Whether RTC time should be treated as stale after I2C failure.
static bool app_i2cRtcStale() {
  return rtcError || !rtc_hasValidNow();
}

// Sequential initialization of all controller subsystems.
void app_setup() {
  app_forceOutputsOff();

  storage_begin();

  i2c_setPeripheralReinit(app_reinitPeripherals);
  i2c_setRtcStalePredicate(app_i2cRtcStale);
  i2c_init(DEBUG_SERIAL != 0);
  delay(100);
  i2c_probeDevices(DEBUG_SERIAL != 0);
  delay(50);

  app_serialBegin();
  Serial.println();
  Serial.println(F("=== Controller boot ==="));
  Serial.println(FIRMWARE_VERSION);
  app_logBootInfo();

  app_confirmBootOk();

  if (storage_isReady()) {
    app_log(F("Settings NVS: OK (on-chip flash)"));
  } else {
    app_log(F("Settings NVS: FAILED"));
  }

  if (i2c_isRtcPresent()) {
    app_log(F("RTC on I2C bus (probe)"));
  } else {
    app_log(F("RTC missing on I2C (probe)"));
  }

  app_log(F("GPIO init"));
  app_initLightPwm();
  input_backend_begin();
  app_log(F("Encoder ready"));

  if (i2c_isOledPresent()) {
    disp_begin(i2c_getOledAddress());
  } else {
    disp_begin(OLED_I2C_ADDR);
  }
  i2c_softReinit();
  delay(30);
  disp_wake();
  app_log(F("Display init done"));

  if (!sensors_initDs18()) {
    ds18State.active = false;
    ds18State.dataValid = false;
    app_log(F("DS18B20 not found"));
  } else {
    app_log(F("DS18B20 OK"));
  }

  if (!sensors_initSht30()) {
    app_log(F("SHT30 not found"));
  } else {
    app_log(F("SHT30 OK"));
  }

  if (!rtc_initHardware()) {
    app_log(F("RTC not found (post-SHT)"));
  } else {
    app_log(F("RTC OK (post-SHT)"));
  }

  if (!storage_isReady()) {
    storage_loadDefaults();
    app_log(F("Settings: NVS unavailable, RAM defaults"));
  } else if (!storage_load()) {
    storage_logLoadFailure();
    storage_loadDefaults();
    if (storage_save()) {
      app_log(F("Settings: defaults saved to NVS"));
    } else {
      storage_logSaveFailure();
      app_log(F("Settings: NVS save failed"));
    }
  } else {
    app_log(F("Settings: loaded from NVS"));
  }

  logic_beginBootHold();

  unsigned long nowMs = millis();
  fanCycleMs = nowMs;
  humCycleMs = nowMs;
  humMaxStartMs = nowMs;
  heatMaxStartMs = nowMs;
  incMaxStartMs = nowMs;

  lastActivity = millis();
  splashActive = true;
  splashStartMillis = millis();
  ui_requestRedraw();

  rgb_status_begin();
  app_log(F("RGB status LED ready"));

  app_applyPersistedSettings();
  system_info_init();
  wifi_manager_begin();
  sensor_log_begin();
  if (wifi_manager_isCompiledIn()) {
    if (wifi_manager_isActive()) {
      app_log(F("WiFi SoftAP started (see Serial for SSID)"));
    } else {
      app_log(F("WiFi module ready (AP not started)"));
    }
  }

  app_watchdogEnable();
  app_log(F("=== Setup complete ==="));
  app_watchdogReset();
}
