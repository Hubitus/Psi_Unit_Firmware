/*
 * app_loop.cpp — main program loop
 *
 * Each tick (every few milliseconds):
 * poll sensors and buttons → compute outputs → draw screen → Wi‑Fi/log.
 * Call order matters: gather data, decide, then render.
 */

#include "app_loop.h"
#include "app_platform.h"
#include "app_state.h"
#include "config.h"
#include "storage.h"
#include "sensors.h"
#include "rtc_clock.h"
#include "logic.h"
#include "display_ui.h"
#include "app_effects.h"
#include "i2c_bus.h"
#include "ui_input.h"
#include "rgb_status.h"
#include "system_info.h"
#include "serial_cli.h"
#include "app_snapshot.h"
#include "display_backend.h"
#include "input_backend.h"
#include "wifi_manager.h"
#include "sensor_log.h"

static AppSnapshot s_frameSnap;
static InputEvent s_inputEvents[4];

// USB, CLI, I2C health check, and deferred settings write to flash.
static void app_pollServices(unsigned long nowMillis) {
  app_serialPollHost();
  app_pollHeapWarning(nowMillis);
  serial_cli_poll();
  i2c_poll(nowMillis, ui_isOledBusBusy());
  storage_pollSave();
}

// RTC service (DST, screen cache).
static void app_pollRtc(unsigned long nowMillis) {
  rtc_service(nowMillis);
  rtc_updateCache();
}

// Boot splash: show main screen after SPLASH_DURATION_MS.
static void app_pollSplash(unsigned long nowMillis) {
  if (splashActive && (nowMillis - splashStartMillis >= SPLASH_DURATION_MS)) {
    splashActive = false;
    app_applyPersistedSettings();
    ui_requestRedraw();
  }
}

// OLED sleep after prolonged inactivity (Slp setting in menu).
static void app_pollDisplaySleep(unsigned long nowMillis) {
  if (!isSleep && sleepTimeout != 0xFFFFFFFF && (nowMillis - lastActivity > sleepTimeout)) {
    ui_sleepDisplay();
  }
}

// Encoder and button → events (short/long press, rotation).
static void app_pollInput(unsigned long nowMillis) {
  const uint8_t count = input_backend_poll(nowMillis, s_inputEvents, 4);
  for (uint8_t i = 0; i < count; i++) {
    ui_input_handleEvent(s_inputEvents[i]);
  }
}

// Per-channel output decision (heat, humidity, …) and GPIO drive.
static void app_runLogic(unsigned long nowMillis) {
  logic_pollBootHold(nowMillis);

  if (logic_isCriticalFault()) {
    logic_onCriticalFault();
    logic_applyOutputPins();
    return;
  }

  if (logic_isNegativeTempSafety()) {
    logic_onNegativeTempSafety();
    logic_applyOutputPins();
    return;
  }

  logic_controlHeat(nowMillis);
  logic_controlHum(nowMillis);
  logic_controlFan(nowMillis);
  logic_controlLight();
  logic_controlInc(nowMillis);

  logic_applyFailsafe(nowMillis);
  logic_applyOutputPins();
}

// Refresh Sys Info screen once per second when user is viewing it.
static void app_pollSystemInfo(unsigned long nowMillis) {
  if (system_info_tick(nowMillis) && uiState == UI_SYSTEM_INFO) {
    system_info_refresh();
    ui_requestRedraw();
  }
}

// One OLED frame: main, menu, QR, etc.
static void app_drawDisplayFrame() {
  ui_applyOledTheme();
  if (splashActive) {
    ui_drawSplash();
    return;
  }
  switch (uiState) {
    case UI_MAIN:
      app_buildSnapshot(s_frameSnap);
      ui_drawMain(s_frameSnap);
      break;
    case UI_MAIN_MENU:
      ui_drawMainMenu();
      break;
    case UI_SUB_MENU:
      ui_drawSubMenu();
      break;
    case UI_EDIT:
      ui_drawEdit();
      break;
    case UI_CLOCK_EDIT:
      ui_drawClockEdit();
      break;
    case UI_CONFIRM_RESET:
      ui_drawResetConfirm();
      break;
    case UI_SYSTEM_INFO:
      ui_drawSystemInfo();
      break;
    case UI_WIFI_QR:
      ui_drawWifiQr();
      break;
  }
  if (uiState == UI_SUB_MENU) {
    ui_drawToastOverlayIfActive(millis());
  }
}

// Redraw main screen when sensors or outputs change.
static void app_pollSnapshotRedraw() {
  if (app_consumeMainScreenDirty()) {
    ui_requestRedrawIfMainVisible();
  }
}

// OLED redraw at most every DISPLAY_INTERVAL ms (saves I2C and CPU).
static void app_pollDisplay(unsigned long nowMillis) {
  if (!isSleep && !splashActive && uiState == UI_MAIN && rtc_hasValidNow() &&
      (nowMillis - lastClockRedraw >= 1000UL)) {
    lastClockRedraw = nowMillis;
    ui_requestRedraw();
  }

  if (ui_toastPollExpiry(nowMillis)) {
    ui_requestRedraw();
  }

  if (!isSleep && ui_needsRedraw() && (nowMillis - lastDisplay >= DISPLAY_INTERVAL)) {
    ui_setOledBusBusy(true);
    disp_renderPages(app_drawDisplayFrame);
    ui_setOledBusBusy(false);
    lastDisplay = nowMillis;
    ui_clearRedrawFlag();
  }
}

void app_loop() {
  app_watchdogReset();
  unsigned long nowMillis = millis();

  app_pollServices(nowMillis);
  app_pollSplash(nowMillis);
  app_pollDisplaySleep(nowMillis);
  sensors_poll(nowMillis);
  app_pollRtc(nowMillis);
  app_pollSystemInfo(nowMillis);
  sensor_updateErrorFlags();
  ui_input_pollInactivity(nowMillis);
  app_pollInput(nowMillis);
  app_runLogic(nowMillis);
  app_pollSnapshotRedraw();
  app_pollDisplay(nowMillis);
  rgb_status_poll(nowMillis);
  wifi_manager_poll(nowMillis);
  sensor_log_poll(nowMillis);
}
