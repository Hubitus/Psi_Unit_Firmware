/*
 * ui_input.cpp — menu logic: button press and encoder rotation handlers
 *
 * Each screen (main, menu, edit, clock, etc.) has short press, long press, and
 * encoder handlers. The UI_SM table maps UiState to these handlers.
 */

#include "ui_input.h"
#include "display_ui.h"
#include "app_effects.h"
#include "app_commands.h"
#include "input_backend.h"
#include "settings_field.h"
#include "rgb_status.h"
#include "rtc_clock.h"
#include "system_info.h"
#include "app_state.h"
#include "config.h"
#include <string.h>

static void ui_returnToMainScreen() {
  uiState = UI_MAIN;
  if (!ui_isOledBusBusy()) {
    rtc_updateCache(true);
  }
}

// --- Main screen and main menu handlers ---

static void sm_main_short() {
  uiState = UI_MAIN_MENU;
  mainMenuIndex = 0;
}

static void sm_main_long() {
  app_dispatchApply(CMD_TOGGLE_LIGHT_OVERRIDE, nullptr);
}

static void sm_mainMenu_short() {
  if (mainMenuIndex == MM_EXIT) {
    ui_returnToMainScreen();
  } else {
    currentSection = (MainMenuItem)mainMenuIndex;
    ui_closeSettingsDisplayMenu();
    ui_closeSettingsRgbMenu();
    ui_closeSettingsWifiMenu();
    ui_closeSettingsLogMenu();
    uiState = UI_SUB_MENU;
    subMenuIndex = 0;
    subMenuOffset = 0;
  }
}

static void sm_mainMenu_long() {
  ui_returnToMainScreen();
}

static void sm_mainMenu_enc(int delta) {
  mainMenuIndex = constrain(mainMenuIndex + delta, 0, MAIN_MENU_COUNT - 1);
}

static void sm_subMenu_short() {
  const MenuItem *items = ui_getSectionItems(currentSection);
  memcpy_P(&currentItem, &items[subMenuIndex], sizeof(MenuItem));

  if (currentItem.type == IT_WIFI_QR) {
    ui_wifiQrBegin();
    uiState = UI_WIFI_QR;
    return;
  }

  if (currentItem.type == IT_BACK) {
    if (currentSection == MM_SETTINGS && ui_isSettingsDisplayMenuActive()) {
      ui_closeSettingsDisplayMenu();
      subMenuIndex = 0;
      subMenuOffset = 0;
    } else if (currentSection == MM_SETTINGS && ui_isSettingsRgbMenuActive()) {
      ui_closeSettingsRgbMenu();
      subMenuIndex = 0;
      subMenuOffset = 0;
    } else if (currentSection == MM_SETTINGS && ui_isSettingsWifiMenuActive()) {
      ui_closeSettingsWifiMenu();
      subMenuIndex = 0;
      subMenuOffset = 0;
    } else if (currentSection == MM_SETTINGS && ui_isSettingsLogMenuActive()) {
      ui_closeSettingsLogMenu();
      subMenuIndex = 0;
      subMenuOffset = 0;
    } else {
      uiState = UI_MAIN_MENU;
    }
  } else if (currentItem.type == IT_SAVE) {
    ui_logMenuApplyDrafts();
    ui_toastShow(millis());
  } else if (currentItem.type == IT_SUBMENU) {
    if (currentSection == MM_SETTINGS) {
      if (subMenuIndex == SM_SET_DISPLAY) {
        ui_openSettingsDisplayMenu();
        subMenuIndex = 0;
        subMenuOffset = 0;
      } else if (subMenuIndex == SM_SET_RGB) {
        ui_openSettingsRgbMenu();
        subMenuIndex = 0;
        subMenuOffset = 0;
      } else if (subMenuIndex == SM_SET_WIFI) {
        ui_openSettingsWifiMenu();
        subMenuIndex = 0;
        subMenuOffset = 0;
#if SENSOR_LOG_ENABLE
      } else if (subMenuIndex == SM_SET_LOG) {
        ui_openSettingsLogMenu();
        subMenuIndex = 0;
        subMenuOffset = 0;
#endif
      } else if (subMenuIndex == SM_SET_SYSINFO) {
        uiState = UI_SYSTEM_INFO;
        subMenuIndex = 0;
        subMenuOffset = 0;
        system_info_refresh();
      }
    }
  } else if (currentItem.type == IT_CLEAR) {
    editValue = 0;
    uiState = UI_CONFIRM_RESET;
  } else if (currentItem.type == IT_CLOCK_SET) {
    uiState = UI_CLOCK_EDIT;
    ui_loadEdit();
  } else {
    uiState = UI_EDIT;
    ui_loadEdit();
  }
}

static void sm_subMenu_long() {
  if (currentSection == MM_SETTINGS && ui_isSettingsDisplayMenuActive()) {
    ui_closeSettingsDisplayMenu();
    subMenuIndex = 0;
    subMenuOffset = 0;
  } else if (currentSection == MM_SETTINGS && ui_isSettingsRgbMenuActive()) {
    ui_closeSettingsRgbMenu();
    subMenuIndex = 0;
    subMenuOffset = 0;
  } else if (currentSection == MM_SETTINGS && ui_isSettingsWifiMenuActive()) {
    ui_closeSettingsWifiMenu();
    subMenuIndex = 0;
    subMenuOffset = 0;
  } else if (currentSection == MM_SETTINGS && ui_isSettingsLogMenuActive()) {
    ui_closeSettingsLogMenu();
    subMenuIndex = 0;
    subMenuOffset = 0;
  } else {
    uiState = UI_MAIN_MENU;
  }
}

static void sm_subMenu_enc(int delta) {
  uint8_t count = ui_getSubMenuCount(currentSection);
  subMenuIndex = constrain(subMenuIndex + delta, 0, count - 1);

  if (subMenuIndex < subMenuOffset) {
    subMenuOffset = subMenuIndex;
  } else if (subMenuIndex >= subMenuOffset + MENU_VISIBLE_LINES) {
    subMenuOffset = subMenuIndex - (MENU_VISIBLE_LINES - 1);
  }
}

static void sm_edit_short() {
  if (currentItem.field == SF_RGB_BRIGHTNESS) {
    rgb_status_setBrightnessPreview(0, false);
  }
  const bool saved = ui_applyEdit();
  uiState = UI_SUB_MENU;
  if (saved) {
    ui_toastShow(millis());
  }
  ui_requestRedraw();
}

static void sm_edit_long() {
  editValue = editBackup;
  if (currentItem.field == SF_RGB_BRIGHTNESS) {
    rgb_status_setBrightnessPreview(0, false);
    rgb_status_applySettings();
  }
  uiState = UI_SUB_MENU;
}

static void sm_edit_enc(int delta) {
  if (currentItem.field == SF_WIFI_TIMEOUT) {
    static const int16_t kWifiTimeouts[] = {WIFI_AP_SESSION_NEVER, 30, 60, 120, WIFI_AP_SESSION_MIN_MAX};
    int8_t idx = 0;
    for (uint8_t i = 0; i < WIFI_AP_SESSION_CHOICE_COUNT; i++) {
      if (editValue == kWifiTimeouts[i]) {
        idx = (int8_t)i;
        break;
      }
    }
    idx = (int8_t)(idx + delta);
    if (idx < 0) {
      idx = (int8_t)(WIFI_AP_SESSION_CHOICE_COUNT - 1);
    } else if (idx >= (int8_t)WIFI_AP_SESSION_CHOICE_COUNT) {
      idx = 0;
    }
    editValue = kWifiTimeouts[idx];
    return;
  }

  if (currentItem.field == SF_LOG_INTERVAL) {
    static const int16_t kLogIntervals[] = {5, 10, 15};
    int8_t idx = 0;
    for (uint8_t i = 0; i < 3; i++) {
      if (editValue == kLogIntervals[i]) {
        idx = (int8_t)i;
        break;
      }
    }
    idx = (int8_t)(idx + delta);
    if (idx < 0) {
      idx = 2;
    } else if (idx > 2) {
      idx = 0;
    }
    editValue = kLogIntervals[idx];
    return;
  }

  int16_t step = settings_fieldEncoderStep(currentItem.field);
  editValue += delta * step;
  editValue = constrain(editValue, currentItem.minv, currentItem.maxv);
  if (settings_fieldIsBrightnessPercent(currentItem.field)) {
    if (currentItem.field == SF_BRIGHTNESS) {
      ui_applyOledContrast(settings_fieldBrightnessPercentToRaw((uint8_t)editValue, SF_BRIGHTNESS));
    } else if (currentItem.field == SF_RGB_BRIGHTNESS) {
      rgb_status_setBrightnessPreview(settings_fieldBrightnessPercentToRaw((uint8_t)editValue, SF_RGB_BRIGHTNESS),
                                      true);
      rgb_status_applySettings();
    }
  }
}

static void sm_confirm_short() {
  if (editValue > 0) {
    app_dispatchApply(CMD_RESET_SETTINGS, nullptr);
  }
  uiState = UI_MAIN_MENU;
  editValue = 0;
}

static void sm_confirm_long() {
  editValue = 0;
  uiState = UI_SUB_MENU;
}

static void sm_confirm_enc(int delta) {
  (void)delta;
  editValue = (editValue == 0) ? 1 : 0;
}

static void sm_clock_short() {
  if (ui_clockEditOnShortPress()) {
    uiState = UI_SUB_MENU;
    if (ui_clockEditWasSaved()) {
      ui_toastShow(millis());
    }
    ui_requestRedraw();
  } else {
    ui_requestRedraw();
  }
}

static void sm_clock_long() {
  if (ui_clockEditOnLongPress()) {
    uiState = UI_SUB_MENU;
  }
}

static void sm_clock_enc(int delta) {
  ui_clockEditOnEncoder(delta);
}

static void sm_sysinfo_exit() {
  uiState = UI_SUB_MENU;
  subMenuIndex = SM_SET_SYSINFO;
  subMenuOffset = 0;
}

static void sm_sysinfo_short() {
  if (subMenuIndex == (int8_t)(ui_sysinfoDisplayCount() - 1)) {
    sm_sysinfo_exit();
  }
}

static void sm_sysinfo_long() {
  sm_sysinfo_exit();
}

static void sm_sysinfo_enc(int delta) {
  const uint8_t count = ui_sysinfoDisplayCount();
  subMenuIndex = constrain(subMenuIndex + delta, 0, count - 1);

  if (subMenuIndex < subMenuOffset) {
    subMenuOffset = subMenuIndex;
  } else if (subMenuIndex >= subMenuOffset + SYSINFO_VISIBLE_LINES) {
    subMenuOffset = subMenuIndex - (SYSINFO_VISIBLE_LINES - 1);
  }
}

static void sm_wifiqr_exit() {
  uiState = UI_SUB_MENU;
}

static void sm_wifiqr_short() {
  sm_wifiqr_exit();
}

static void sm_wifiqr_long() {
  sm_wifiqr_exit();
}

static void sm_wifiqr_enc(int delta) {
  ui_wifiQrOnEncoder(delta);
}

// Table: screen → actions on SHORT / LONG / ENCODER.
typedef void (*UiHandler)(void);
typedef void (*UiEncHandler)(int);

struct UiStateMachine {
  UiHandler onShort;
  UiHandler onLong;
  UiEncHandler onEncoder;
};

static const UiStateMachine UI_SM[] = {
  /* UI_MAIN */          {sm_main_short,     sm_main_long,      nullptr},
  /* UI_MAIN_MENU */     {sm_mainMenu_short, sm_mainMenu_long,  sm_mainMenu_enc},
  /* UI_SUB_MENU */      {sm_subMenu_short,  sm_subMenu_long,   sm_subMenu_enc},
  /* UI_EDIT */          {sm_edit_short,     sm_edit_long,      sm_edit_enc},
  /* UI_CLOCK_EDIT */    {sm_clock_short,    sm_clock_long,     sm_clock_enc},
  /* UI_CONFIRM_RESET */ {sm_confirm_short,  sm_confirm_long,   sm_confirm_enc},
  /* UI_SYSTEM_INFO */   {sm_sysinfo_short,  sm_sysinfo_long,   sm_sysinfo_enc},
  /* UI_WIFI_QR */       {sm_wifiqr_short,   sm_wifiqr_long,    sm_wifiqr_enc},
};

static const UiStateMachine &ui_sm() {
  return UI_SM[uiState];
}

// Auto-return from settings editing after prolonged inactivity.
void ui_input_pollInactivity(unsigned long nowMillis) {
  if (uiState == UI_MAIN || (nowMillis - lastActivity < EDIT_INACTIVITY_TIMEOUT)) {
    return;
  }

  if (uiState == UI_EDIT) {
    if (currentItem.field == SF_RGB_BRIGHTNESS) {
      rgb_status_setBrightnessPreview(0, false);
      rgb_status_applySettings();
    }
    editValue = editBackup;
  } else if (uiState == UI_CLOCK_EDIT) {
    ui_clockEditCancel();
  } else if (uiState == UI_CONFIRM_RESET) {
    editValue = 0;
  }
  if (currentSection == MM_SETTINGS) {
    ui_closeSettingsDisplayMenu();
    ui_closeSettingsRgbMenu();
    ui_closeSettingsWifiMenu();
    ui_closeSettingsLogMenu();
  }
  ui_returnToMainScreen();
  ui_requestRedraw();
}

void ui_input_onShortPress() {
  const UiHandler fn = ui_sm().onShort;
  if (fn != nullptr) {
    fn();
  }
  ui_requestRedraw();
}

void ui_input_onLongPress() {
  const UiHandler fn = ui_sm().onLong;
  if (fn != nullptr) {
    fn();
  }
  ui_requestRedraw();
}

void ui_input_onEncoder(int delta) {
  if (delta == 0) {
    return;
  }

  ui_wakeUp();

  const UiEncHandler fn = ui_sm().onEncoder;
  if (fn != nullptr) {
    fn(delta);
  }

  if (uiState != UI_CLOCK_EDIT) {
    ui_requestRedraw();
  }
}

// Entry point: input_backend event → current screen handler.
void ui_input_handleEvent(const InputEvent &ev) {
  switch (ev.type) {
    case INPUT_WAKE:
      ui_wakeUp();
      ui_requestRedraw();
      break;
    case INPUT_SHORT_PRESS:
      ui_input_onShortPress();
      break;
    case INPUT_LONG_PRESS:
      ui_input_onLongPress();
      break;
    case INPUT_ENCODER:
      ui_input_onEncoder(ev.delta);
      break;
    default:
      break;
  }
}
