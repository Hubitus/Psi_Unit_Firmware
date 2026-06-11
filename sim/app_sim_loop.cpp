/*
 * app_sim_loop.cpp — UI simulator loop (no sensors / serial_cli / watchdog)
 */

#include "app_loop.h"
#include "app_state.h"
#include "config.h"
#include "storage.h"
#include "rtc_clock.h"
#include "logic.h"
#include "display_ui.h"
#include "app_effects.h"
#include "ui_input.h"
#include "app_snapshot.h"
#include "display_backend.h"
#include "input_backend.h"
#include "rgb_status.h"
#include "system_info.h"

static AppSnapshot s_frameSnap;
static InputEvent s_inputEvents[4];

static void app_pollSplash(unsigned long nowMillis) {
  if (splashActive && (nowMillis - splashStartMillis >= SPLASH_DURATION_MS)) {
    splashActive = false;
    app_applyPersistedSettings();
    lastClockRedraw = nowMillis - 1000UL;
    ui_requestRedraw();
  }
}

static void app_pollDisplaySleep(unsigned long nowMillis) {
  if (!isSleep && sleepTimeout != 0xFFFFFFFF && (nowMillis - lastActivity > sleepTimeout)) {
    ui_sleepDisplay();
  }
}

static void app_pollInput(unsigned long nowMillis) {
  const uint8_t count = input_backend_poll(nowMillis, s_inputEvents, 4);
  for (uint8_t i = 0; i < count; i++) {
    ui_input_handleEvent(s_inputEvents[i]);
  }
}

static void app_runLogic(unsigned long nowMillis) {
  logic_pollBootHold(nowMillis);

  if (logic_isCriticalFault()) {
    logic_onCriticalFault();
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

static void app_pollSnapshotRedraw() {
  if (app_consumeMainScreenDirty()) {
    ui_requestRedrawIfMainVisible();
  }
}

static void app_pollDisplay(unsigned long nowMillis) {
  if (!isSleep && !splashActive && uiState == UI_MAIN && rtc_hasValidNow()) {
    rtc_updateCache();
    static uint8_t s_lastDrawnSecond = 255;
    const uint8_t sec = rtc_getNow().second();
    if (sec != s_lastDrawnSecond) {
      s_lastDrawnSecond = sec;
      lastClockRedraw = nowMillis;
      ui_requestRedraw();
    }
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
  const unsigned long nowMillis = millis();
  storage_pollSave();
  app_pollSplash(nowMillis);
  app_pollDisplaySleep(nowMillis);
  if (system_info_tick(nowMillis) && uiState == UI_SYSTEM_INFO) {
    system_info_refresh();
    ui_requestRedraw();
  }
  ui_input_pollInactivity(nowMillis);
  app_pollInput(nowMillis);
  app_runLogic(nowMillis);
  app_pollSnapshotRedraw();
  app_pollDisplay(nowMillis);
  rgb_status_poll(nowMillis);
}
