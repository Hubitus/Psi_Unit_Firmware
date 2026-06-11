/*
 * app_sim_setup.cpp — simulator initialization (no I2C / sensors)
 */

#include "app_setup.h"
#include "app_platform.h"
#include "app_state.h"
#include "config.h"
#include "storage.h"
#include "rtc_clock.h"
#include "logic.h"
#include "display_ui.h"
#include "app_effects.h"
#include "display_backend.h"
#include "input_backend.h"
#include "rgb_status.h"
#include "system_info.h"

void app_setup() {
  app_forceOutputsOff();
  storage_begin();
  app_serialBegin();
  Serial.println();
  Serial.println(F("=== Psi Unit UI Simulator ==="));
  Serial.println(FIRMWARE_VERSION);
  app_logBootInfo();

  input_backend_begin();
  disp_begin(OLED_I2C_ADDR);
  disp_wake();

  rtc_initHardware();
  storage_load();

  logic_beginBootHold();

  const unsigned long nowMs = millis();
  fanCycleMs = nowMs;
  humCycleMs = nowMs;
  humMaxStartMs = nowMs;
  heatMaxStartMs = nowMs;
  incMaxStartMs = nowMs;
  lastActivity = nowMs;
  splashActive = true;
  splashStartMillis = nowMs;
  ui_requestRedraw();

  rgb_status_begin();
  app_applyPersistedSettings();
  system_info_init();
  Serial.println(F("=== Setup complete (sim) ==="));
}
