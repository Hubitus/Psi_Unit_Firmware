/*
 * display_ui.cpp — OLED 128×64 screen: menu, main screen, and all settings views
 *
 * Renders what the user sees on the display: splash on power-up, main screen with
 * sensors and output status, settings menu, value editing, clock, Wi-Fi QR code,
 * and diagnostics screen. Does not manage sensors — only displays the state
 * snapshot (AppSnapshot) and receives commands from ui_input.
 */

#include "display_ui.h"
#include "app_snapshot.h"
#include "app_commands.h"
#include "app_effects.h"
#include "display_backend.h"
#include "rtc_clock.h"
#include "system_info.h"
#include "settings_field.h"
#include "settings_access.h"
#include "splash.h"
#include "wifi_icon.h"
#include "app_state.h"
#include "wifi_manager.h"
#include "wifi_qr.h"
#include "config.h"
#include <Arduino.h>
#include <stdio.h>
#include <string.h>

static bool needRedraw = true;
static bool oledBusBusy = false;

// --- OLED redraw flags ---

void ui_requestRedraw() {
  needRedraw = true;
}

void ui_requestRedrawIfMainVisible() {
  if (!isSleep && !splashActive && uiState == UI_MAIN) {
    needRedraw = true;
  }
}

bool ui_needsRedraw() {
  return needRedraw;
}

void ui_clearRedrawFlag() {
  needRedraw = false;
}

bool ui_isOledBusBusy() {
  return oledBusBusy;
}

void ui_setOledBusBusy(bool busy) {
  oledBusBusy = busy;
}

static void ui_drawStrP(uint8_t x, uint8_t y, PGM_P pstr) {
  disp_drawStrP(x, y, pstr);
}

UiState uiState = UI_MAIN;
int8_t mainMenuIndex = 0;
MainMenuItem currentSection = MM_INC;
int8_t subMenuIndex = 0;
int8_t subMenuOffset = 0;
MenuItem currentItem;
int16_t editValue = 0;
int16_t editBackup = 0;
bool clockEditingField = false;
int8_t currentClockField = 0;
int16_t clockEditValue = 0;
static bool settingsDisplayMenuActive = false;
static bool settingsRgbMenuActive = false;
static bool settingsWifiMenuActive = false;
static bool settingsLogMenuActive = false;
static bool s_logDraftEnabled = true;
#if SENSOR_LOG_ENABLE
static uint8_t s_logDraftIntervalMin = SENSOR_LOG_INTERVAL_MIN;
#else
static uint8_t s_logDraftIntervalMin = 5;
#endif
static uint8_t s_wifiQrPage = 0;
static unsigned long s_toastUntilMs = 0;
static bool s_clockJustSaved = false;

static const uint8_t kScrollbarX = 125;
static const uint8_t kSubMenuRowH = 10;
static const uint8_t kSubMenuLineStepCompact = 10;
static const uint8_t kSubMenuLineStepRelaxed = 12;
static const uint8_t kMenuTitleBarH = 13;
static const uint8_t kMenuTitleTextY = 10;
static const uint8_t kMenuTitleDotR = 1;
static const uint8_t kMenuTitleDotGap = 4;

static void ui_drawMenuTitle(const char *title, uint8_t barWidth = 128) {
  disp_setFontUi();
  const uint8_t titleW = disp_getStrWidth(title);
  const uint8_t dotD = (uint8_t)(kMenuTitleDotR * 2);
  const uint8_t adornW = (uint8_t)(titleW + 2 * (dotD + kMenuTitleDotGap));
  const uint8_t startX = (adornW < barWidth) ? (uint8_t)((barWidth - adornW) / 2) : 0;
  const uint8_t titleX = (uint8_t)(startX + dotD + kMenuTitleDotGap);
  const uint8_t dotY = 7;

  disp_setDrawColor(1);
  disp_drawBox(0, 0, barWidth, kMenuTitleBarH);
  disp_setDrawColor(0);
  disp_drawDisc((uint8_t)(startX + kMenuTitleDotR), dotY, kMenuTitleDotR);
  disp_drawStr(titleX, kMenuTitleTextY, title);
  disp_drawDisc((uint8_t)(startX + adornW - kMenuTitleDotR - 1), dotY, kMenuTitleDotR);
  disp_setDrawColor(1);
}

static void ui_drawMenuTitleP(PGM_P title, uint8_t barWidth = 128) {
  char buf[32];
  strncpy_P(buf, title, sizeof(buf) - 1);
  buf[sizeof(buf) - 1] = '\0';
  ui_drawMenuTitle(buf, barWidth);
}

static bool ui_usesRelaxedSubMenuLayout() {
  if (currentSection == MM_INC || currentSection == MM_HEAT) {
    return true;
  }
  return currentSection == MM_SETTINGS &&
         (settingsWifiMenuActive || settingsRgbMenuActive || settingsLogMenuActive);
}

static uint8_t ui_subMenuLineStep() {
  return ui_usesRelaxedSubMenuLayout() ? kSubMenuLineStepRelaxed : kSubMenuLineStepCompact;
}

const char txt_saved[] PROGMEM = "Saved";

void ui_toastShow(unsigned long nowMillis) {
  s_toastUntilMs = nowMillis + UI_TOAST_DURATION_MS;
}

bool ui_toastIsActive(unsigned long nowMillis) {
  return s_toastUntilMs != 0 && nowMillis < s_toastUntilMs;
}

bool ui_toastPollExpiry(unsigned long nowMillis) {
  if (s_toastUntilMs != 0 && nowMillis >= s_toastUntilMs) {
    s_toastUntilMs = 0;
    return true;
  }
  return false;
}

static void ui_drawScrollbar(uint8_t pos, uint8_t total) {
  if (total <= 1) {
    return;
  }
  const uint8_t yTop = 0;
  const uint8_t height = DISP_HEIGHT;
  disp_setDrawColor(0);
  disp_drawBox(kScrollbarX, yTop, 3, height);
  disp_setDrawColor(1);
  for (uint8_t y = yTop; y < height; y += 2) {
    disp_drawPixel((uint8_t)(kScrollbarX + 1), y);
  }
  uint8_t thumbH = (uint8_t)(height / total);
  if (thumbH < 2) {
    thumbH = 2;
  }
  if (thumbH > height) {
    thumbH = height;
  }
  const uint8_t travel = (uint8_t)(height - thumbH);
  const uint8_t thumbY = (uint8_t)(yTop + (uint16_t)travel * pos / (total - 1));
  disp_drawBox(kScrollbarX, thumbY, 3, thumbH);
}

static void ui_drawToastOverlay() {
  // Same font and color scheme as selected menu item (6x10 + inversion).
  // u8g2_font_10x20 does not render on SH1106 page-buffer here — white fill only.
  disp_setMaxClipWindow();
  disp_setFontUi();

  char msg[8];
  strcpy_P(msg, txt_saved);
  const uint8_t textW = disp_getStrWidth(msg);
  const uint8_t boxH = 14;
  const uint8_t boxW = (uint8_t)(textW + 16);
  const uint8_t x = (uint8_t)((DISP_WIDTH - boxW) / 2);
  const uint8_t yTop = (uint8_t)((DISP_HEIGHT - boxH) / 2);
  const uint8_t ty = (uint8_t)(yTop + boxH - 2);
  const uint8_t tx = (uint8_t)(x + (boxW - textW) / 2);

  disp_setDrawColor(1);
  disp_drawBox(x, yTop, boxW, boxH);
  disp_setDrawColor(0);
  disp_drawFrame(x, yTop, boxW, boxH);
  disp_drawStr(tx, ty, msg);
  disp_drawStr((uint8_t)(tx + 1), ty, msg);
  disp_setDrawColor(1);
}

void ui_drawToastOverlayIfActive(unsigned long nowMillis) {
  if (ui_toastIsActive(nowMillis)) {
    ui_drawToastOverlay();
  }
}

// --- Menu labels and items (stored in flash to save RAM) ---

const char mm0[] PROGMEM = "INC";
const char mm1[] PROGMEM = "GRH";
const char mm2[] PROGMEM = "HUM";
const char mm3[] PROGMEM = "FAN";
const char mm4[] PROGMEM = "LGT";
const char mm5[] PROGMEM = "SET";
const char mm6[] PROGMEM = "BACK";
const char * const mainMenuTxt[] PROGMEM = { mm0, mm1, mm2, mm3, mm4, mm5, mm6 };

const char sm0[] PROGMEM = "Mod";
const char sm1[] PROGMEM = "Tmp";
const char sm2[] PROGMEM = "THy";
const char sm3[] PROGMEM = "Hum";
const char sm4[] PROGMEM = "HHy";
const char sm5[] PROGMEM = "Max";
const char sm_wrk[] PROGMEM = "Wrk";
const char sm_rst[] PROGMEM = "Rst";
const char sm_unt[] PROGMEM = "Unt";
const char sm9[] PROGMEM = "Str";
const char sm_ld[] PROGMEM = "Dur";
const char sm_lp[] PROGMEM = "Pct";
const char sm_cs[] PROGMEM = "TIME";
const char sm_dsp[] PROGMEM = "DISP";
const char sm_rgb[] PROGMEM = "RGB";
const char sm_wifi[] PROGMEM = "WiFi";
const char sm_wifi_ap[] PROGMEM = "Wi-Fi SoftAP";
const char sm_wifi_qr[] PROGMEM = "QR-code";
const char sm_wifi_tmo[] PROGMEM = "Timeout";
const char txt_never[] PROGMEM = "Never";
const char sm_log[] PROGMEM = "LOG";
const char sm_log_en[] PROGMEM = "Enable";
const char sm_log_int[] PROGMEM = "Interval";
const char sm_sys[] PROGMEM = "SYS";
const char sm_reset[] PROGMEM = "RESET";
const char sm_rgb_en[] PROGMEM = "Enable";
const char sm_rgb_brt[] PROGMEM = "Brightness";
const char sm_save[] PROGMEM = "Save";
const char sm16[] PROGMEM = "Slp";
const char sm17[] PROGMEM = "Brt";
const char sm_dsp_slp[] PROGMEM = "Sleep";
const char sm_dsp_brt[] PROGMEM = "Brightness";
const char sm_dsp_rot[] PROGMEM = "Rotate";
const char sm_dsp_inv[] PROGMEM = "Invert";
const char sm_back[] PROGMEM = "BACK";
const char sm_lgt_mode[] PROGMEM = "Mode";
const char sm_lgt_start[] PROGMEM = "Start";
const char sm_lgt_dur[] PROGMEM = "Duration";
const char sm_lgt_brt[] PROGMEM = "Brightness";
const char sm20[] PROGMEM = "Rot";
const char sm_inv[] PROGMEM = "Inv";
const char mode_auto[] PROGMEM = "AUTO";
const char mode_on[] PROGMEM = "ON";
const char mode_off[] PROGMEM = "OFF";
const char mode_man[] PROGMEM = "MAN";
const char * const ui_modeNames[] PROGMEM = { mode_auto, mode_on, mode_off, mode_man };
const char dev_inc[] PROGMEM = "INC";
const char dev_grh[] PROGMEM = "GRH";
const char dev_hum[] PROGMEM = "HUM";
const char dev_fan[] PROGMEM = "FAN";
const char dev_lgt[] PROGMEM = "LGT";
const char txt_err[] PROGMEM = "ERR!";
const char txt_rtc_err[] PROGMEM = "RTC ERROR!";
const char txt_reset_q[] PROGMEM = "RESET SETTINGS?";

const MenuItem heatItems[] PROGMEM = {
  {sm0, IT_MODE, SF_HEAT_MODE, 0, 2},
  {sm1, IT_INT, SF_T_TARGET, TEMP_SET_MIN, TEMP_SET_MAX},
  {sm2, IT_HYST, SF_T_HYST, TEMP_HYST_MIN, TEMP_HYST_MAX},
  {sm_back, IT_BACK, SF_NONE, 0, 0}
};

const MenuItem humItems[] PROGMEM = {
  {sm0, IT_MODE, SF_HUM_MODE, 0, 3},
  {sm3, IT_INT, SF_H_TARGET, HUM_SET_MIN, HUM_SET_MAX},
  {sm4, IT_INT, SF_H_HYST, 1, 20},
  {sm5, IT_INT, SF_HUM_MAX, 1, 250},
  {sm_wrk, IT_INT, SF_HUM_WORK, 1, 250},
  {sm_unt, IT_UNIT, SF_HUM_WORK_UNIT, 0, 1},
  {sm_rst, IT_INT, SF_HUM_REST, 1, 250},
  {sm_unt, IT_UNIT, SF_HUM_REST_UNIT, 0, 1},
  {sm_back, IT_BACK, SF_NONE, 0, 0}
};

const MenuItem fanItems[] PROGMEM = {
  {sm_wrk, IT_INT, SF_FAN_WORK, 1, 250},
  {sm_unt, IT_UNIT, SF_FAN_WORK_UNIT, 0, 1},
  {sm_rst, IT_INT, SF_FAN_REST, 1, 250},
  {sm_unt, IT_UNIT, SF_FAN_REST_UNIT, 0, 1},
  {sm0, IT_MODE, SF_FAN_MODE, 0, 2},
  {sm_back, IT_BACK, SF_NONE, 0, 0}
};

const MenuItem lightItems[] PROGMEM = {
  {sm_lgt_mode, IT_MODE, SF_LIGHT_MODE, 0, 2},
  {sm_lgt_start, IT_INT, SF_LIGHT_HOUR, 0, 23},
  {sm_lgt_dur, IT_INT, SF_LIGHT_DURATION, 1, 24},
  {sm_lgt_brt, IT_INT, SF_LIGHT_BRIGHTNESS, 0, 100},
  {sm_back, IT_BACK, SF_NONE, 0, 0}
};

const MenuItem incItems[] PROGMEM = {
  {sm0, IT_MODE, SF_INC_MODE, 0, 2},
  {sm1, IT_INT, SF_INC_TARGET, TEMP_SET_MIN, TEMP_SET_MAX},
  {sm2, IT_HYST, SF_INC_HYST, TEMP_HYST_MIN, TEMP_HYST_MAX},
  {sm_back, IT_BACK, SF_NONE, 0, 0}
};

const MenuItem settingsItems[] PROGMEM = {
  {sm_wifi, IT_SUBMENU, SF_NONE, 0, 0},
  {sm_rgb, IT_SUBMENU, SF_NONE, 0, 0},
  {sm_dsp, IT_SUBMENU, SF_NONE, 0, 0},
  {sm_cs, IT_CLOCK_SET, SF_NONE, 0, 0},
#if SENSOR_LOG_ENABLE
  {sm_log, IT_SUBMENU, SF_NONE, 0, 0},
#endif
  {sm_sys, IT_SUBMENU, SF_NONE, 0, 0},
  {sm_reset, IT_CLEAR, SF_NONE, 0, 0},
  {sm_back, IT_BACK, SF_NONE, 0, 0}
};

#if WIFI_ENABLE
const MenuItem wifiSettingsItems[] PROGMEM = {
  {sm_wifi_ap, IT_BOOL, SF_WIFI_AP_ENABLED, 0, 1},
  {sm_wifi_qr, IT_WIFI_QR, SF_NONE, 0, 0},
  {sm_wifi_tmo, IT_INT, SF_WIFI_TIMEOUT, WIFI_AP_SESSION_NEVER, WIFI_AP_SESSION_MIN_MAX},
  {sm_back, IT_BACK, SF_NONE, 0, 0}
};
#else
const MenuItem wifiSettingsItems[] PROGMEM = {
  {sm_back, IT_BACK, SF_NONE, 0, 0}
};
#endif

const MenuItem rgbLedItems[] PROGMEM = {
  {sm_rgb_en, IT_BOOL, SF_RGB_LED_ENABLED, 0, 1},
  {sm_rgb_brt, IT_INT, SF_RGB_BRIGHTNESS, 0, 100},
  {sm_back, IT_BACK, SF_NONE, 0, 0}
};

const MenuItem displayItems[] PROGMEM = {
  {sm_dsp_slp, IT_INT, SF_SLEEP_MIN, 0, 60},
  {sm_dsp_brt, IT_INT, SF_BRIGHTNESS, 0, 100},
  {sm_dsp_rot, IT_BOOL, SF_DISPLAY_ROTATED, 0, 1},
  {sm_dsp_inv, IT_BOOL, SF_DISPLAY_INVERTED, 0, 1},
  {sm_back, IT_BACK, SF_NONE, 0, 0}
};

#if SENSOR_LOG_ENABLE
const MenuItem logSettingsItems[] PROGMEM = {
  {sm_log_en, IT_BOOL, SF_LOG_ENABLED, 0, 1},
  {sm_log_int, IT_INT, SF_LOG_INTERVAL, 5, 15},
  {sm_save, IT_SAVE, SF_NONE, 0, 0},
  {sm_back, IT_BACK, SF_NONE, 0, 0}
};
#endif

const MenuItem * const sectionItems[] PROGMEM = {
  incItems, heatItems, humItems, fanItems, lightItems, settingsItems
};

const uint8_t sectionSizes[] PROGMEM = {
  4, 4, 9, 6, 5,
#if SENSOR_LOG_ENABLE
  8
#else
  7
#endif
};

uint8_t ui_getSubMenuCount(MainMenuItem section) {
  if (section == MM_SETTINGS && settingsDisplayMenuActive) {
    return sizeof(displayItems) / sizeof(displayItems[0]);
  }
  if (section == MM_SETTINGS && settingsRgbMenuActive) {
    return sizeof(rgbLedItems) / sizeof(rgbLedItems[0]);
  }
  if (section == MM_SETTINGS && settingsWifiMenuActive) {
    return sizeof(wifiSettingsItems) / sizeof(wifiSettingsItems[0]);
  }
#if SENSOR_LOG_ENABLE
  if (section == MM_SETTINGS && settingsLogMenuActive) {
    return sizeof(logSettingsItems) / sizeof(logSettingsItems[0]);
  }
#endif
  return pgm_read_byte(&sectionSizes[section]);
}

const MenuItem* ui_getSectionItems(MainMenuItem section) {
  if (section == MM_SETTINGS && settingsDisplayMenuActive) {
    return displayItems;
  }
  if (section == MM_SETTINGS && settingsRgbMenuActive) {
    return rgbLedItems;
  }
  if (section == MM_SETTINGS && settingsWifiMenuActive) {
    return wifiSettingsItems;
  }
#if SENSOR_LOG_ENABLE
  if (section == MM_SETTINGS && settingsLogMenuActive) {
    return logSettingsItems;
  }
#endif
  return (const MenuItem*)pgm_read_ptr(&sectionItems[section]);
}

bool ui_isSettingsDisplayMenuActive() {
  return settingsDisplayMenuActive;
}

void ui_openSettingsDisplayMenu() {
  settingsDisplayMenuActive = true;
}

void ui_closeSettingsDisplayMenu() {
  settingsDisplayMenuActive = false;
}

bool ui_isSettingsRgbMenuActive() {
  return settingsRgbMenuActive;
}

void ui_openSettingsRgbMenu() {
  settingsRgbMenuActive = true;
}

void ui_closeSettingsRgbMenu() {
  settingsRgbMenuActive = false;
}

bool ui_isSettingsWifiMenuActive() {
  return settingsWifiMenuActive;
}

void ui_openSettingsWifiMenu() {
  settingsWifiMenuActive = true;
}

void ui_closeSettingsWifiMenu() {
  settingsWifiMenuActive = false;
}

bool ui_isSettingsLogMenuActive() {
  return settingsLogMenuActive;
}

void ui_openSettingsLogMenu() {
  settingsLogMenuActive = true;
  s_logDraftEnabled = settings_accessGetUInt8(SF_LOG_ENABLED) != 0;
  s_logDraftIntervalMin = settings_accessGetUInt8(SF_LOG_INTERVAL);
}

void ui_closeSettingsLogMenu() {
  settingsLogMenuActive = false;
}

void ui_logMenuApplyDrafts() {
  const AppSetLogPayload payload = {true, s_logDraftEnabled, true, s_logDraftIntervalMin};
  app_dispatchApply(CMD_SET_LOG, &payload);
}

static uint8_t ui_logIntervalSnap(uint8_t minutes) {
  if (minutes <= 7) {
    return 5;
  }
  if (minutes <= 12) {
    return 10;
  }
  return 15;
}

static bool ui_isLogSettingsField(SettingsFieldId field) {
  return field == SF_LOG_ENABLED || field == SF_LOG_INTERVAL;
}

void ui_clockEditBegin() {
  currentClockField = CLOCK_FIELD_YEAR;
  clockEditingField = false;
  rtc_beginClockEdit();
}

void ui_clockEditCancel() {
  clockEditingField = false;
  rtc_cancelClockEdit();
}

static void ui_clockEditApplyPending() {
  if (!clockEditingField || currentClockField > CLOCK_FIELD_MINUTE) {
    return;
  }
  rtc_applyEditField(currentClockField, clockEditValue);
  clockEditingField = false;
}

bool ui_clockEditWasSaved() {
  return s_clockJustSaved;
}

bool ui_clockEditOnShortPress() {
  s_clockJustSaved = false;
  if (rtcError) {
    ui_clockEditCancel();
    return true;
  }
  if (currentClockField == CLOCK_FIELD_SAVE) {
    ui_clockEditApplyPending();
    const bool ok = app_dispatchApply(CMD_COMMIT_RTC, nullptr);
    s_clockJustSaved = ok;
    return ok;
  }
  if (currentClockField == CLOCK_FIELD_BACK) {
    ui_clockEditCancel();
    return true;
  }
  if (clockEditingField) {
    rtc_applyEditField(currentClockField, clockEditValue);
    clockEditingField = false;
    if (currentClockField < CLOCK_FIELD_MINUTE) {
      currentClockField = (int8_t)(currentClockField + 1);
    } else {
      currentClockField = CLOCK_FIELD_SAVE;
    }
    return false;
  }
  clockEditingField = true;
  clockEditValue = rtc_getEditField(currentClockField);
  return false;
}

bool ui_clockEditOnLongPress() {
  ui_clockEditCancel();
  return true;
}

void ui_clockEditOnEncoder(int delta) {
  if (delta == 0) {
    return;
  }
  if (clockEditingField) {
    int16_t minv, maxv;
    rtc_getFieldLimits(currentClockField, minv, maxv);
    clockEditValue += delta;
    clockEditValue = constrain(clockEditValue, minv, maxv);
  } else {
    int8_t next = currentClockField + delta;
    if (next < 0) {
      next = CLOCK_FIELD_COUNT - 1;
    } else if (next >= CLOCK_FIELD_COUNT) {
      next = 0;
    }
    currentClockField = next;
  }
  ui_requestRedraw();
}

void ui_sleepDisplay() {
  if (isSleep) {
    return;
  }
  isSleep = true;
  disp_clear();
  ui_clearRedrawFlag();
}

void ui_wakeUp() {
  if (isSleep) {
    isSleep = false;
    disp_wake();
    app_applyPersistedSettings();
    ui_requestRedraw();
  }
  app_touchActivity(millis());
}

const char* ui_getModeShort(Mode mode) {
  switch (mode) {
    case MODE_AUTO: return "AUT";
    case MODE_FORCE_ON: return "ON";
    case MODE_FORCE_OFF: return "OFF";
    case MODE_MANUAL: return "MAN";
    default: return "?";
  }
}

// --- Settings editing: load, apply, reset ---

void ui_loadEdit() {
  if (currentItem.type == IT_MODE || currentItem.type == IT_INT ||
      currentItem.type == IT_HYST || currentItem.type == IT_UNIT || currentItem.type == IT_BOOL) {
    if (ui_isSettingsLogMenuActive() && currentItem.field == SF_LOG_ENABLED) {
      editValue = s_logDraftEnabled ? 1 : 0;
    } else if (ui_isSettingsLogMenuActive() && currentItem.field == SF_LOG_INTERVAL) {
      editValue = s_logDraftIntervalMin;
    } else if (settings_fieldIsTempDeci(currentItem.field)) {
      editValue = settings_accessGetInt16(currentItem.field);
    } else if (settings_fieldIsBrightnessPercent(currentItem.field)) {
      editValue = settings_fieldBrightnessRawToPercent(settings_fieldGetUInt8(currentItem.field), currentItem.field);
    } else if (currentItem.type == IT_BOOL) {
      editValue = settings_accessGetUInt8(currentItem.field) ? 1 : 0;
    } else {
      editValue = settings_accessGetUInt8(currentItem.field);
    }
    editBackup = editValue;
  } else if (currentItem.type == IT_CLOCK_SET) {
    ui_clockEditBegin();
  }
}

void ui_applyOledContrast(uint8_t rawContrast) {
  disp_setContrast(rawContrast);
  disp_setInverted(settings_fieldGetUInt8(SF_DISPLAY_INVERTED) != 0);
}

void ui_applyOledTheme() {
  ui_applyOledContrast(settings_fieldGetUInt8(SF_BRIGHTNESS));
}

void ui_applyDisplaySettings() {
  disp_setRotation180(settings_fieldGetUInt8(SF_DISPLAY_ROTATED) != 0);
  ui_applyOledTheme();
}

void ui_resetSettingsToDefaults() {
  app_dispatchApply(CMD_RESET_SETTINGS, nullptr);
}

bool ui_applyEdit() {
  if (ui_isSettingsLogMenuActive() && ui_isLogSettingsField(currentItem.field)) {
    if (currentItem.field == SF_LOG_ENABLED) {
      s_logDraftEnabled = (editValue != 0);
    } else {
      s_logDraftIntervalMin = ui_logIntervalSnap((uint8_t)editValue);
    }
    return false;
  }
  if (currentItem.type == IT_MODE || currentItem.type == IT_INT ||
      currentItem.type == IT_HYST || currentItem.type == IT_UNIT || currentItem.type == IT_BOOL) {
    const AppSetFieldPayload payload = {currentItem.field, currentItem.type, editValue};
    return app_dispatchApply(CMD_SET_FIELD, &payload);
  }
  if (currentItem.type == IT_CLEAR) {
    return app_dispatchApply(CMD_RESET_SETTINGS, nullptr);
  }
  return false;
}

static void ui_formatTempDeci(char *buf, int16_t deciC) {
  itoa(deciC / 10, buf, 10);
  const size_t len = strlen(buf);
  buf[len] = '.';
  buf[len + 1] = '0' + (char)abs(deciC % 10);
  buf[len + 2] = 'C';
  buf[len + 3] = '\0';
}

static void ui_formatHystDeci(char *buf, uint8_t deciC) {
  buf[0] = (char)('0' + deciC / 10);
  buf[1] = '.';
  buf[2] = (char)('0' + deciC % 10);
  buf[3] = 'C';
  buf[4] = '\0';
}

void ui_formatValue(char* buf, MenuItem &item) {
  if (item.field == SF_LOG_ENABLED) {
    const bool enabled = ui_isSettingsLogMenuActive() ? s_logDraftEnabled : (settings_accessGetUInt8(SF_LOG_ENABLED) != 0);
    if (enabled) {
      strcpy_P(buf, mode_on);
    } else {
      strcpy_P(buf, mode_off);
    }
  } else if (item.field == SF_LOG_INTERVAL) {
    const uint8_t v = ui_isSettingsLogMenuActive() ? s_logDraftIntervalMin : settings_accessGetUInt8(SF_LOG_INTERVAL);
    itoa(v, buf, 10);
    const uint8_t len = strlen(buf);
    buf[len] = 'm';
    buf[len + 1] = 0;
  } else if (item.type == IT_MODE) {
    strcpy(buf, ui_getModeShort((Mode)settings_accessGetUInt8(item.field)));
  } else if (item.type == IT_BOOL) {
    if (settings_accessGetUInt8(item.field)) {
      strcpy_P(buf, mode_on);
    } else {
      strcpy_P(buf, mode_off);
    }
  } else if (item.type == IT_HYST) {
    ui_formatHystDeci(buf, settings_accessGetUInt8(item.field));
  } else if (item.type == IT_UNIT) {
    strcpy(buf, settings_accessGetUInt8(item.field) == UNIT_SECONDS ? "sec" : "min");
  } else if (item.type == IT_INT) {
    if (settings_fieldIsTempDeci(item.field)) {
      ui_formatTempDeci(buf, settings_accessGetInt16(item.field));
    } else {
      uint8_t v = settings_accessGetUInt8(item.field);
      if (settings_fieldIsBrightnessPercent(item.field)) {
        v = settings_fieldBrightnessRawToPercent(v, item.field);
      }
      if (item.field == SF_SLEEP_MIN && v == 0) {
        strcpy_P(buf, mode_off);
      } else if (item.field == SF_WIFI_TIMEOUT && v == 0) {
        strcpy_P(buf, txt_never);
      } else {
        itoa(v, buf, 10);
        uint8_t len = strlen(buf);
        if (item.field == SF_H_TARGET || item.field == SF_H_HYST) {
          buf[len] = '%';
          buf[len + 1] = 0;
        } else if (item.field == SF_LIGHT_HOUR || item.field == SF_LIGHT_DURATION) {
          buf[len] = 'h';
          buf[len + 1] = 0;
        } else if (item.field == SF_SLEEP_MIN || item.field == SF_HUM_MAX || item.field == SF_WIFI_TIMEOUT) {
          buf[len] = 'm';
          buf[len + 1] = 0;
        } else if (item.field == SF_BRIGHTNESS || item.field == SF_RGB_BRIGHTNESS ||
                   item.field == SF_LIGHT_BRIGHTNESS) {
          buf[len] = '%';
          buf[len + 1] = 0;
        } else if (item.field == SF_HUM_WORK || item.field == SF_HUM_REST ||
                   item.field == SF_FAN_WORK || item.field == SF_FAN_REST) {
          TimeUnit unit = settings_fieldCycleUnit(item.field);
          buf[len] = (unit == UNIT_SECONDS) ? 's' : 'm';
          buf[len + 1] = 0;
        }
      }
    }
  }
}

// --- Screen rendering ---

void ui_drawSplash() {
  disp_setFontUi();
  const int8_t logoX = (128 - SPLASH_LOGO_WIDTH) / 2;
  const int8_t logoY = (64 - SPLASH_LOGO_HEIGHT) / 2 - 2;
  disp_drawXBMP(logoX, logoY, SPLASH_LOGO_WIDTH, SPLASH_LOGO_HEIGHT, splashLogoBits);
  disp_setDrawColor(2);
  disp_drawBox(logoX, logoY, SPLASH_LOGO_WIDTH, SPLASH_LOGO_HEIGHT);
  disp_setDrawColor(1);
  const char* ver = FIRMWARE_VERSION;
  uint8_t verW = disp_getStrWidth(ver);
  disp_drawStr((128 - verW) / 2, 63, ver);
}

static void ui_drawWifiIndicator(uint8_t centerX, uint8_t centerY) {
  static const unsigned long kBlinkMs = 1000UL;
  if (((millis() / kBlinkMs) & 1U) != 0U) {
    return;
  }
  disp_drawXBMP((uint8_t)(centerX - WIFI_ICON_WIDTH / 2), (uint8_t)(centerY - WIFI_ICON_HEIGHT / 2),
                WIFI_ICON_WIDTH, WIFI_ICON_HEIGHT, wifiIconBits);
}

// Main screen layout coordinates (top — sensors, bottom — outputs).
static const uint8_t kMainTopDivY = 13;
static const uint8_t kMainBotDivY = 42;
static const uint8_t kSensorDivX = 64;
static const uint8_t kSensorLineGap = 2;
static const uint8_t kSensorVLineTop = kMainTopDivY + kSensorLineGap + 1;
static const uint8_t kSensorVLineBot = kMainBotDivY - kSensorLineGap - 1;
static const uint8_t kIncValueX = 25;
static const uint8_t kIncRowY = 30;
static const uint8_t kGrhRowY = 25;
static const uint8_t kHumRowY = 37;
static const uint8_t kSensorLabelValueGap = 2;

static void ui_drawDashedVLine(uint8_t x, uint8_t y0, uint8_t y1) {
  for (uint8_t y = y0; y <= y1; y += 2) {
    disp_drawPixel(x, y);
  }
  // Draw bottom pixel if line length is odd.
  if (((y1 - y0) & 1U) != 0U) {
    disp_drawPixel(x, y1);
  }
}

static void ui_drawMainSensorRightRow(uint8_t y, const char *label, const char *value) {
  const uint8_t valW = disp_getStrWidth(value);
  const uint8_t valX = (uint8_t)(128 - valW);
  const uint8_t lblW = disp_getStrWidth(label);
  uint8_t lblX = (uint8_t)(valX - lblW - kSensorLabelValueGap);
  const uint8_t minLblX = (uint8_t)(kSensorDivX + kSensorLineGap + 1);
  if (lblX < minLblX) {
    lblX = minLblX;
  }
  disp_drawStr(lblX, y, label);
  disp_drawStr(valX, y, value);
}

static void ui_drawMainSensorRightRowP(uint8_t y, const char *label, PGM_P value) {
  const uint8_t valW = disp_getStrWidthP(value);
  const uint8_t valX = (uint8_t)(128 - valW);
  const uint8_t lblW = disp_getStrWidth(label);
  uint8_t lblX = (uint8_t)(valX - lblW - kSensorLabelValueGap);
  const uint8_t minLblX = (uint8_t)(kSensorDivX + kSensorLineGap + 1);
  if (lblX < minLblX) {
    lblX = minLblX;
  }
  disp_drawStr(lblX, y, label);
  disp_drawStr(valX, y, value);
}

static void ui_appendTemp10(char *buf, int16_t value) {
  ui_formatTempDeci(buf, value);
}

static void ui_appendHumPct(char *buf, int16_t value) {
  itoa(value, buf + strlen(buf), 10);
  const uint8_t len = strlen(buf);
  buf[len] = '%';
  buf[len + 1] = 0;
}

static const uint8_t kMainOutRow0Y = 53;
static const uint8_t kMainOutRow1Y = 63;

static void ui_drawMainOutputCell(uint8_t col, uint8_t row, PGM_P label, bool on, Mode mode,
                                  bool lightOverride) {
  char buf[16];
  const char *modeLabel = ui_getModeShort(mode);
  if (lightOverride) {
    modeLabel = "OVR";
  }
  strcpy_P(buf, label);
  uint8_t len = strlen(buf);
  buf[len] = ':';
  strcpy(buf + len + 1, modeLabel);

  const uint8_t y = (row == 0) ? kMainOutRow0Y : kMainOutRow1Y;
  const uint8_t textW = disp_getStrWidth(buf);
  uint8_t x;
  if (col == 0) {
    x = 0;
  } else if (col == 1) {
    x = (uint8_t)((128 - textW) / 2);
  } else {
    x = (uint8_t)(128 - textW);
  }

  if (on) {
    disp_setDrawColor(1);
    disp_drawBox(x, y - 8, textW, 10);
    disp_setDrawColor(0);
    disp_drawStr(x, y, buf);
    disp_setDrawColor(1);
  } else {
    disp_drawStr(x, y, buf);
  }
}

static void ui_drawMainOutputCenterStack(PGM_P label, bool on, Mode mode, bool lightOverride) {
  const char *modeLabel = ui_getModeShort(mode);
  if (lightOverride) {
    modeLabel = "OVR";
  }
  char nameBuf[8];
  strcpy_P(nameBuf, label);
  const uint8_t nameW = disp_getStrWidth(nameBuf);
  const uint8_t modeW = disp_getStrWidth(modeLabel);
  const uint8_t nameX = (uint8_t)((128 - nameW) / 2);
  const uint8_t modeX = (uint8_t)((128 - modeW) / 2);

  if (on) {
    disp_setDrawColor(1);
    disp_drawBox(nameX - 2, kMainOutRow0Y - 8, nameW + 4, 10);
    disp_drawBox(modeX - 2, kMainOutRow1Y - 8, modeW + 4, 10);
    disp_setDrawColor(0);
  }
  disp_drawStr(nameX, kMainOutRow0Y, nameBuf);
  disp_drawStr(modeX, kMainOutRow1Y, modeLabel);
  disp_setDrawColor(1);
}

// Main screen: time, INC/GRH sensors, Wi-Fi indicator, output grid.
void ui_drawMain(const AppSnapshot &snap) {
  char buf[16];
  disp_setFontUi();
  if (snap.rtc.ok) {
    buf[0] = '0' + snap.rtc.hour / 10;
    buf[1] = '0' + snap.rtc.hour % 10;
    buf[2] = ':';
    buf[3] = '0' + snap.rtc.minute / 10;
    buf[4] = '0' + snap.rtc.minute % 10;
    buf[5] = ':';
    buf[6] = '0' + snap.rtc.second / 10;
    buf[7] = '0' + snap.rtc.second % 10;
    buf[8] = 0;
    static const uint8_t kClockY = 10;
    static const uint8_t kTimeX = 0;
    const uint8_t timeW = disp_getStrWidth(buf);
    disp_drawStr(kTimeX, kClockY, buf);
    const uint8_t year2 = (uint8_t)(snap.rtc.year % 100U);
    buf[0] = '0' + snap.rtc.day / 10;
    buf[1] = '0' + snap.rtc.day % 10;
    buf[2] = '/';
    buf[3] = '0' + snap.rtc.month / 10;
    buf[4] = '0' + snap.rtc.month % 10;
    buf[5] = '/';
    buf[6] = '0' + year2 / 10;
    buf[7] = '0' + year2 % 10;
    buf[8] = 0;
    const uint8_t dateW = disp_getStrWidth(buf);
    const uint8_t dateX = (uint8_t)(128 - dateW);
    disp_drawStr(dateX, kClockY, buf);
    if (wifi_manager_isCompiledIn() && wifi_manager_isActive()) {
      const uint8_t timeEndX = (uint8_t)(kTimeX + timeW);
      const uint8_t centerX = (uint8_t)((timeEndX + dateX) / 2);
      const uint8_t centerY = 5;
      ui_drawWifiIndicator(centerX, centerY);
    }
  } else {
    ui_drawStrP(0, 10, txt_rtc_err);
  }
  if (!snap.nvsReady) {
    disp_drawStr(0, 63, "NO NVS");
  }
  for (uint8_t x = 0; x < 128; x += 2) {
    disp_drawPixel(x, kMainTopDivY);
  }
  ui_drawDashedVLine(kSensorDivX, kSensorVLineTop, kSensorVLineBot);
  disp_drawStr(0, kIncRowY, "INC:");
  if (snap.incTemp.valid) {
    ui_formatTempDeci(buf, snap.incTemp.value);
    disp_drawStr(kIncValueX, kIncRowY, buf);
  } else {
    ui_drawStrP(kIncValueX, kIncRowY, txt_err);
  }
  if (snap.grhTemp.valid) {
    buf[0] = 0;
    ui_appendTemp10(buf, snap.grhTemp.value);
    ui_drawMainSensorRightRow(kGrhRowY, "GRH:", buf);
  } else {
    ui_drawMainSensorRightRowP(kGrhRowY, "GRH:", txt_err);
  }
  if (snap.grhHum.valid) {
    buf[0] = 0;
    ui_appendHumPct(buf, snap.grhHum.value);
    ui_drawMainSensorRightRow(kHumRowY, "HUM:", buf);
  } else {
    ui_drawMainSensorRightRowP(kHumRowY, "HUM:", txt_err);
  }
  for (uint8_t x = 0; x < 128; x += 2) {
    disp_drawPixel(x, kMainBotDivY);
  }
  // Bottom zone — 3×2 grid with output states:
  //   [ INC ] [ LGT ] [ GRH ]
  //   [ FAN ] [  —  ] [ HUM ]
  ui_drawMainOutputCell(0, 0, dev_inc, snap.outputs.incOn, snap.outputs.incMode, false);
  ui_drawMainOutputCenterStack(dev_lgt, snap.outputs.lightOn, snap.outputs.lightMode, snap.outputs.lightOverride);
  ui_drawMainOutputCell(2, 0, dev_grh, snap.outputs.heatOn, snap.outputs.heatMode, false);
  ui_drawMainOutputCell(0, 1, dev_fan, snap.outputs.fanOn, snap.outputs.fanMode, false);
  ui_drawMainOutputCell(2, 1, dev_hum, snap.outputs.humOn, snap.outputs.humMode, false);
}

void ui_drawMainMenu() {
  char label[8];
  ui_drawMenuTitle("MENU");
  const uint8_t cols = 3;
  const uint8_t colW = 42;
  const uint8_t rows = (uint8_t)((MAIN_MENU_COUNT + cols - 1) / cols);
  for (uint8_t row = 0; row < rows; row++) {
    const uint8_t y = (uint8_t)(row * 16 + 26);
    for (uint8_t col = 0; col < cols; col++) {
      const uint8_t idx = (uint8_t)(row * cols + col);
      if (idx >= MAIN_MENU_COUNT) {
        continue;
      }
      strcpy_P(label, (char *)pgm_read_ptr(&mainMenuTxt[idx]));
      const uint8_t colStart = (uint8_t)(col * colW);
      const uint8_t textW = disp_getStrWidth(label);
      const uint8_t x = (uint8_t)(colStart + (colW - textW) / 2);
      if (idx == (uint8_t)mainMenuIndex) {
        disp_setDrawColor(1);
        disp_drawBox(colStart, y - 12, colW, 14);
        disp_setDrawColor(0);
      } else {
        disp_setDrawColor(1);
      }
      disp_drawStr(x, y, label);
    }
    disp_setDrawColor(1);
  }
}

void ui_drawSubMenu() {
  char title[16], val[16], cell[24];
  disp_setFontUi();
  if (currentSection == MM_HUM || currentSection == MM_FAN) {
    strcpy_P(title, (char*)pgm_read_ptr(&mainMenuTxt[currentSection]));
    ui_drawMenuTitle(title);
    uint8_t count = ui_getSubMenuCount(currentSection);
    const MenuItem *items = ui_getSectionItems(currentSection);
    uint8_t rows = (count + 1) / 2;
    const uint8_t gridVisibleRows = 3;
    uint8_t startRow = 0;
    if (currentSection == MM_HUM && rows > gridVisibleRows) {
      startRow = subMenuOffset / 2;
      uint8_t maxStartRow = rows - gridVisibleRows;
      if (startRow > maxStartRow) startRow = maxStartRow;
    }
    uint8_t endRow = rows;
    if (endRow > startRow + gridVisibleRows) endRow = startRow + gridVisibleRows;
    for (uint8_t row = startRow; row < endRow; row++) {
      uint8_t y = (row - startRow) * 16 + 26;
      for (uint8_t col = 0; col < 2; col++) {
        uint8_t idx = row * 2 + col;
        if (idx >= count) continue;
        MenuItem item;
        memcpy_P(&item, &items[idx], sizeof(MenuItem));
        strcpy_P(title, (char*)item.label);
        strcpy(cell, title);
        if (item.type != IT_BACK && item.type != IT_CLEAR && item.type != IT_CLOCK_SET &&
            item.type != IT_SUBMENU && item.type != IT_WIFI_QR && item.type != IT_SAVE) {
          ui_formatValue(val, item);
          strcat(cell, ": ");
          strcat(cell, val);
        }
        uint8_t colStart = col * 64;
        if (idx == subMenuIndex) {
          disp_setDrawColor(1);
          disp_drawBox(colStart, y - 12, 64, 14);
          disp_setDrawColor(0);
        } else disp_setDrawColor(1);
        disp_drawStr(colStart + 3, y, cell);
      }
      disp_setDrawColor(1);
    }
    if (currentSection == MM_HUM && rows > gridVisibleRows) {
      ui_drawScrollbar(startRow, (uint8_t)(rows - gridVisibleRows + 1));
    }
    return;
  }
  if (currentSection == MM_SETTINGS && !settingsDisplayMenuActive && !settingsRgbMenuActive &&
      !settingsWifiMenuActive && !settingsLogMenuActive) {
    ui_drawMenuTitle("SET");
    const uint8_t count = ui_getSubMenuCount(currentSection);
    const MenuItem *items = ui_getSectionItems(currentSection);
    const uint8_t cols = 3;
    const uint8_t colW = 42;
    const uint8_t rows = (uint8_t)((count + cols - 1) / cols);
    for (uint8_t row = 0; row < rows; row++) {
      const uint8_t y = (uint8_t)(row * 16 + 26);
      for (uint8_t col = 0; col < cols; col++) {
        const uint8_t idx = (uint8_t)(row * cols + col);
        if (idx >= count) {
          continue;
        }
        MenuItem item;
        memcpy_P(&item, &items[idx], sizeof(MenuItem));
        strcpy_P(title, (char*)item.label);
        const uint8_t colStart = (uint8_t)(col * colW);
        const uint8_t textW = disp_getStrWidth(title);
        const uint8_t x = (uint8_t)(colStart + (colW - textW) / 2);
        if (idx == subMenuIndex) {
          disp_setDrawColor(1);
          disp_drawBox(colStart, y - 12, colW, 14);
          disp_setDrawColor(0);
        } else {
          disp_setDrawColor(1);
        }
        disp_drawStr(x, y, title);
      }
      disp_setDrawColor(1);
    }
    return;
  }
  if (currentSection == MM_SETTINGS && settingsDisplayMenuActive) {
    strcpy(title, "DISP");
  } else if (currentSection == MM_SETTINGS && settingsRgbMenuActive) {
    strcpy(title, "RGB");
  } else if (currentSection == MM_SETTINGS && settingsWifiMenuActive) {
    strcpy(title, "WiFi");
  } else if (currentSection == MM_SETTINGS && settingsLogMenuActive) {
    strcpy(title, "LOG");
  } else if (currentSection == MM_SETTINGS) {
    strcpy(title, "SET");
  } else {
    strcpy_P(title, (char*)pgm_read_ptr(&mainMenuTxt[currentSection]));
  }
  ui_drawMenuTitle(title);
  uint8_t count = ui_getSubMenuCount(currentSection);
  const MenuItem *items = ui_getSectionItems(currentSection);
  const uint8_t lineStep = ui_subMenuLineStep();
  for (uint8_t i = 0; i < MENU_VISIBLE_LINES && (i + subMenuOffset) < count; i++) {
    uint8_t idx = i + subMenuOffset;
    uint8_t y = (uint8_t)(24 + i * lineStep);
    if (idx == subMenuIndex) {
      disp_setDrawColor(1);
      disp_drawBox(0, y - 9, 128, kSubMenuRowH);
      disp_setDrawColor(0);
    }
    MenuItem item;
    memcpy_P(&item, &items[idx], sizeof(MenuItem));
    strcpy_P(title, (char*)item.label);
    disp_drawStr(4, y, title);
    if (item.type != IT_BACK && item.type != IT_CLEAR && item.type != IT_CLOCK_SET &&
        item.type != IT_SUBMENU && item.type != IT_WIFI_QR && item.type != IT_SAVE) {
      ui_formatValue(val, item);
      disp_drawStr(128 - strlen(val) * 6 - 4, y, val);
    }
    if (idx == subMenuIndex) disp_setDrawColor(1);
  }
  if (count > MENU_VISIBLE_LINES) {
    ui_drawScrollbar(subMenuOffset, (uint8_t)(count - MENU_VISIBLE_LINES + 1));
  }
}

void ui_drawEdit() {
  char label[16], val[16], range[32];
  strcpy_P(label, (char*)currentItem.label);
  ui_drawMenuTitle(label);
  if (currentItem.type == IT_MODE) {
    uint8_t idx = constrain(editValue, 0, (currentItem.maxv == 3) ? 3 : 2);
    strcpy_P(val, (PGM_P)pgm_read_ptr(&ui_modeNames[idx]));
  } else if (currentItem.type == IT_BOOL) {
    if (editValue == 1) strcpy_P(val, mode_on);
    else strcpy_P(val, mode_off);
  } else if (currentItem.type == IT_HYST || settings_fieldIsTempDeci(currentItem.field)) {
    ui_formatTempDeci(val, editValue);
  } else if (currentItem.type == IT_UNIT) {
    strcpy(val, (editValue == UNIT_SECONDS) ? "SEC" : "MIN");
  } else {
    if (currentItem.field == SF_SLEEP_MIN && editValue == 0) {
      strcpy_P(val, mode_off);
    } else if (currentItem.field == SF_WIFI_TIMEOUT && editValue == 0) {
      strcpy_P(val, txt_never);
    } else if (currentItem.field == SF_LOG_INTERVAL) {
      itoa(editValue, val, 10);
      const uint8_t len = strlen(val);
      val[len] = 'm';
      val[len + 1] = 0;
    } else {
      itoa(editValue, val, 10);
      uint8_t len = strlen(val);
      if (currentItem.field == SF_H_TARGET || currentItem.field == SF_H_HYST) {
        val[len] = '%';
        val[len + 1] = 0;
      } else if (currentItem.field == SF_LIGHT_HOUR || currentItem.field == SF_LIGHT_DURATION ||
                 currentItem.field == SF_SLEEP_MIN || currentItem.field == SF_HUM_MAX ||
                 currentItem.field == SF_WIFI_TIMEOUT) {
        val[len] = (currentItem.field == SF_LIGHT_HOUR || currentItem.field == SF_LIGHT_DURATION) ? 'h' : 'm';
        val[len + 1] = 0;
      } else if (currentItem.field == SF_BRIGHTNESS || currentItem.field == SF_RGB_BRIGHTNESS ||
                 currentItem.field == SF_LIGHT_BRIGHTNESS) {
        val[len] = '%';
        val[len + 1] = 0;
      } else if (currentItem.field == SF_HUM_WORK || currentItem.field == SF_HUM_REST ||
                 currentItem.field == SF_FAN_WORK || currentItem.field == SF_FAN_REST) {
        TimeUnit unit = settings_fieldCycleUnit(currentItem.field);
        val[len] = (unit == UNIT_SECONDS) ? 's' : 'm';
        val[len + 1] = 0;
      }
    }
  }
  disp_drawStr((128 - disp_getStrWidth(val)) / 2, 35, val);
  disp_setFontUi();
  if (settings_fieldIsTempDeci(currentItem.field) || currentItem.type == IT_HYST) {
    range[0] = '(';
    itoa(currentItem.minv / 10, range + 1, 10);
    uint8_t len = strlen(range);
    range[len] = '.';
    range[len + 1] = '0' + (char)abs(currentItem.minv % 10);
    range[len + 2] = ' ';
    range[len + 3] = '-';
    range[len + 4] = ' ';
    itoa(currentItem.maxv / 10, range + len + 5, 10);
    len = strlen(range);
    range[len] = '.';
    range[len + 1] = '0' + (char)abs(currentItem.maxv % 10);
    range[len + 2] = ')';
    range[len + 3] = '\0';
  } else {
    range[0] = '('; itoa(currentItem.minv, range+1, 10);
    uint8_t len = strlen(range);
    range[len] = ' '; range[len+1] = '-'; range[len+2] = ' ';
    itoa(currentItem.maxv, range+len+3, 10);
    len = strlen(range);
    range[len] = ')'; range[len+1] = 0;
  }
  disp_drawStr(0, 60, range);
}

void ui_drawResetConfirm() {
  const char* noTxt = "NO";
  const char* yesTxt = "YES Reset default!";
  const uint8_t noY = 26, yesY = 38;
  ui_drawMenuTitleP(txt_reset_q);
  uint8_t noX = (128 - disp_getStrWidth(noTxt)) / 2;
  uint8_t yesX = (128 - disp_getStrWidth(yesTxt)) / 2;
  if (editValue <= 0) {
    disp_setDrawColor(1);
    disp_drawBox(0, noY - 10, 128, 12);
    disp_setDrawColor(0);
    disp_drawStr(noX, noY, noTxt);
    disp_setDrawColor(1);
    disp_drawStr(yesX, yesY, yesTxt);
  } else {
    disp_drawStr(noX, noY, noTxt);
    disp_setDrawColor(1);
    disp_drawBox(0, yesY - 10, 128, 12);
    disp_setDrawColor(0);
    disp_drawStr(yesX, yesY, yesTxt);
    disp_setDrawColor(1);
  }
}

static void ui_drawClockActions(bool saveSelected, bool backSelected) {
  char tmp[12];
  const uint8_t y = 54;
  const uint8_t halfW = 64;

  strncpy_P(tmp, sm_save, sizeof(tmp) - 1);
  tmp[sizeof(tmp) - 1] = 0;
  uint8_t saveX = (halfW - disp_getStrWidth(tmp)) / 2;
  if (saveSelected) {
    disp_setDrawColor(1);
    disp_drawBox(0, y - 10, halfW, 12);
    disp_setDrawColor(0);
    disp_drawStr(saveX, y, tmp);
    disp_setDrawColor(1);
  } else {
    disp_drawStr(saveX, y, tmp);
  }

  strncpy_P(tmp, sm_back, sizeof(tmp) - 1);
  tmp[sizeof(tmp) - 1] = 0;
  uint8_t backX = halfW + (halfW - disp_getStrWidth(tmp)) / 2;
  if (backSelected) {
    disp_setDrawColor(1);
    disp_drawBox(halfW, y - 10, halfW, 12);
    disp_setDrawColor(0);
    disp_drawStr(backX, y, tmp);
    disp_setDrawColor(1);
  } else {
    disp_drawStr(backX, y, tmp);
  }
}

void ui_drawClockEdit() {
  if (rtcError) {
    ui_drawMenuTitle("SET CLOCK");
    disp_drawStr(8, 28, "RTC missing");
    disp_drawStr(0, 46, "Short / Hold = exit");
    return;
  }
  char dateBuf[12];
  char timeBuf[8];
  ui_drawMenuTitle("SET CLOCK");

  int16_t vals[5];
  for (int i = 0; i < 5; i++) {
    vals[i] = (clockEditingField && i == currentClockField) ? clockEditValue : rtc_getEditField(i);
  }

  snprintf(dateBuf, sizeof(dateBuf), "%04d.%02d.%02d",
           (int)vals[0], (int)vals[1], (int)vals[2]);
  snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d", (int)vals[3], (int)vals[4]);

  const uint8_t charW = 6;
  uint8_t dateX = (128 - disp_getStrWidth(dateBuf)) / 2;
  uint8_t timeX = (128 - disp_getStrWidth(timeBuf)) / 2;
  disp_drawStr(dateX, 24, dateBuf);
  disp_drawStr(timeX, 38, timeBuf);

  if (currentClockField <= CLOCK_FIELD_MINUTE) {
    uint8_t boxX = 0;
    uint8_t boxW = 0;
    uint8_t boxY = 14;
    switch (currentClockField) {
      case CLOCK_FIELD_YEAR: boxX = dateX; boxW = 4 * charW; break;
      case CLOCK_FIELD_MONTH: boxX = dateX + 5 * charW; boxW = 2 * charW; break;
      case CLOCK_FIELD_DAY: boxX = dateX + 8 * charW; boxW = 2 * charW; break;
      case CLOCK_FIELD_HOUR: boxX = timeX; boxW = 2 * charW; boxY = 28; break;
      case CLOCK_FIELD_MINUTE: boxX = timeX + 3 * charW; boxW = 2 * charW; boxY = 28; break;
      default: break;
    }
    disp_setDrawColor(2);
    disp_drawBox(boxX, boxY, boxW, 10);
    disp_setDrawColor(1);
  }

  ui_drawClockActions(currentClockField == CLOCK_FIELD_SAVE, currentClockField == CLOCK_FIELD_BACK);
}

static const uint8_t kSysinfoContentTop = 22;
static const uint8_t kSysinfoRowH = 10;
static const uint8_t kSysinfoBarH = 9;
static const uint8_t kSysinfoLineStep = 10;
static const uint8_t kSysinfoContentW = SYSINFO_CLIP_RIGHT;

enum SysinfoDisplayKind : uint8_t {
  SYSINFO_DISP_SECTION = 0,
  SYSINFO_DISP_FIELD,
  SYSINFO_DISP_BACK
};

static void ui_drawSysinfoClipped(uint8_t x, uint8_t y, const char *text, uint8_t maxX) {
  char buf[SYSINFO_VALUE_CHARS + 4];
  strncpy(buf, text, sizeof(buf) - 1);
  buf[sizeof(buf) - 1] = '\0';
  while (buf[0] != '\0' && (x + disp_getStrWidth(buf)) > maxX) {
    buf[strlen(buf) - 1] = '\0';
  }
  disp_drawStr(x, y, buf);
}

static uint8_t ui_sysinfoInfoDisplayRows() {
  uint8_t rows = 0;
  const uint8_t infoRows = system_info_rowCount();
  for (uint8_t i = 0; i < infoRows; i++) {
    const SystemInfoRowKind kind = system_info_rowKind(i);
    if (kind == SYSINFO_ROW_DIVIDER) {
      continue;
    }
    rows++;
  }
  return rows;
}

uint8_t ui_sysinfoDisplayCount() {
  return (uint8_t)(ui_sysinfoInfoDisplayRows() + 1);
}

static bool ui_sysinfoResolve(uint8_t displayIdx, SysinfoDisplayKind &kind, uint8_t &srcIdx) {
  const uint8_t count = ui_sysinfoDisplayCount();
  if (displayIdx >= count) {
    return false;
  }
  if (displayIdx == count - 1) {
    kind = SYSINFO_DISP_BACK;
    srcIdx = 0;
    return true;
  }

  uint8_t slot = 0;
  const uint8_t infoRows = system_info_rowCount();
  for (uint8_t i = 0; i < infoRows; i++) {
    const SystemInfoRowKind rowKind = system_info_rowKind(i);
    if (rowKind == SYSINFO_ROW_DIVIDER) {
      continue;
    }
    if (slot == displayIdx) {
      srcIdx = i;
      kind = (rowKind == SYSINFO_ROW_SECTION) ? SYSINFO_DISP_SECTION : SYSINFO_DISP_FIELD;
      return true;
    }
    slot++;
  }
  return false;
}

static uint8_t ui_sysinfoRowBoxY(uint8_t baselineY, uint8_t boxH) {
  return (uint8_t)(baselineY - (boxH - 1));
}

static void ui_drawSysinfoBarRow(uint8_t baselineY, const char *text) {
  const uint8_t boxY = ui_sysinfoRowBoxY(baselineY, kSysinfoBarH);
  const uint8_t textW = disp_getStrWidth(text);
  const uint8_t textX = (uint8_t)((kSysinfoContentW - textW) / 2);

  disp_setDrawColor(1);
  disp_drawBox(0, boxY, kSysinfoContentW, kSysinfoBarH);
  disp_setDrawColor(0);
  disp_drawStr(textX, baselineY, text);
  disp_setDrawColor(1);
}

static void ui_drawSysinfoSelectableRow(uint8_t baselineY, bool selected) {
  if (!selected) {
    return;
  }
  disp_setDrawColor(1);
  disp_drawBox(0, ui_sysinfoRowBoxY(baselineY, kSysinfoRowH), kSysinfoContentW, kSysinfoRowH);
  disp_setDrawColor(0);
}

static void ui_drawSysinfoFieldRow(uint8_t baselineY, const char *label, const char *value, bool selected) {
  ui_drawSysinfoSelectableRow(baselineY, selected);
  disp_drawStr(SYSINFO_LABEL_X, baselineY, label);

  const uint8_t valueMaxW = (uint8_t)(SYSINFO_VALUE_RIGHT - 40);
  char valBuf[SYSINFO_VALUE_CHARS + 4];
  strncpy(valBuf, value, sizeof(valBuf) - 1);
  valBuf[sizeof(valBuf) - 1] = '\0';
  while (valBuf[0] != '\0' && disp_getStrWidth(valBuf) > valueMaxW) {
    valBuf[strlen(valBuf) - 1] = '\0';
  }
  const uint8_t valW = disp_getStrWidth(valBuf);
  uint8_t valX = 0;
  if (valW <= SYSINFO_VALUE_RIGHT) {
    valX = (uint8_t)(SYSINFO_VALUE_RIGHT - valW);
  }
  ui_drawSysinfoClipped(valX, baselineY, valBuf, SYSINFO_CLIP_RIGHT);

  if (selected) {
    disp_setDrawColor(1);
  }
}

void ui_drawSystemInfo() {
  char backLabel[8];
  const uint8_t count = ui_sysinfoDisplayCount();
  const uint8_t visible = SYSINFO_VISIBLE_LINES;
  uint8_t maxOffset = 0;
  if (count > visible) {
    maxOffset = count - visible;
  }
  if (subMenuOffset > maxOffset) {
    subMenuOffset = maxOffset;
  }
  if (subMenuIndex >= (int8_t)count) {
    subMenuIndex = (int8_t)(count - 1);
  }

  ui_drawMenuTitleP(sm_sys, kSysinfoContentW);

  disp_setFontUi();
  disp_setClipWindow(0, kMenuTitleBarH, SYSINFO_CLIP_RIGHT, 63);

  for (uint8_t slot = 0; slot < visible && (slot + subMenuOffset) < count; slot++) {
    const uint8_t displayIdx = (uint8_t)(slot + subMenuOffset);
    const uint8_t y = (uint8_t)(kSysinfoContentTop + slot * kSysinfoLineStep);
    const bool selected = displayIdx == (uint8_t)subMenuIndex;
    SysinfoDisplayKind kind;
    uint8_t srcIdx = 0;
    if (!ui_sysinfoResolve(displayIdx, kind, srcIdx)) {
      continue;
    }

    switch (kind) {
      case SYSINFO_DISP_SECTION:
        ui_drawSysinfoBarRow(y, system_info_rowLabel(srcIdx));
        break;
      case SYSINFO_DISP_FIELD:
        ui_drawSysinfoFieldRow(y, system_info_rowLabel(srcIdx), system_info_rowValue(srcIdx), selected);
        break;
      case SYSINFO_DISP_BACK:
        strcpy_P(backLabel, sm_back);
        ui_drawSysinfoSelectableRow(y, selected);
        {
          const uint8_t textW = disp_getStrWidth(backLabel);
          const uint8_t textX = (uint8_t)((kSysinfoContentW - textW) / 2);
          disp_drawStr(textX, y, backLabel);
        }
        if (selected) {
          disp_setDrawColor(1);
        }
        break;
    }
  }

  disp_setMaxClipWindow();

  if (count > visible) {
    ui_drawScrollbar(subMenuOffset, (uint8_t)(count - visible + 1));
  }
}

void ui_wifiQrBegin() {
  s_wifiQrPage = 0;
}

void ui_wifiQrOnEncoder(int delta) {
  if (delta == 0) {
    return;
  }
  const uint8_t maxPos = ui_wifiQrScrollMax();
  if (maxPos <= 1) {
    return;
  }
  int next = (int)s_wifiQrPage + delta;
  if (next < 0) {
    next = 0;
  } else if (next >= (int)maxPos) {
    next = (int)maxPos - 1;
  }
  s_wifiQrPage = (uint8_t)next;
}

void ui_drawWifiQr() {
  disp_setFontUi();

  if (!wifi_manager_isCompiledIn()) {
    ui_drawMenuTitle("WiFi QR");
    disp_drawStr(0, 30, "WiFi disabled");
    return;
  }
  if (wifi_manager_isUserApDisabled()) {
    ui_drawMenuTitle("WiFi QR");
    disp_drawStr(0, 26, "WiFi is OFF");
    disp_drawStr(0, 38, "SET WiFi: ON");
    return;
  }

  if (s_wifiQrPage == 0) {
    char payload[96];
    disp_setDrawColor(1);
    if (wifi_manager_buildJoinPayload(payload, sizeof(payload))) {
      uint8_t pix = 0;
      if (!wifi_qr_drawFullscreen(payload, &pix)) {
        disp_drawStr(0, 28, "QR error");
      }
    } else {
      disp_drawStr(0, 28, "QR error");
    }
    if (!wifi_manager_isActive()) {
      disp_drawStr(0, 63, "WiFi starting");
    }
    return;
  }

  char line[24];
  char ssid[16];
  char pass[WIFI_AP_PASS_LEN + 1];
  wifi_manager_getApCredentials(ssid, sizeof(ssid), pass, sizeof(pass));

  static const uint8_t infoLineCount = 5;
  const uint8_t textOffset = s_wifiQrPage - 1;
  const uint8_t visible = WIFI_QR_TEXT_VISIBLE;

  ui_drawMenuTitle("WiFi info");

  for (uint8_t i = 0; i < visible && (i + textOffset) < infoLineCount; i++) {
    const uint8_t idx = (uint8_t)(i + textOffset);
    const uint8_t y = (uint8_t)(24 + i * 10);
    switch (idx) {
      case 0:
        snprintf(line, sizeof(line), "S:%s", ssid);
        break;
      case 1:
        snprintf(line, sizeof(line), "P:%s", pass);
        break;
      case 2:
        snprintf(line, sizeof(line), "U:%s", wifi_manager_getApIp());
        break;
      case 3:
        snprintf(line, sizeof(line), "http://%s", wifi_manager_getApIp());
        break;
      default:
        strncpy(line, "Dial: QR page", sizeof(line));
        line[sizeof(line) - 1] = '\0';
        break;
    }
    disp_drawStr(2, y, line);
  }

  if (infoLineCount > visible) {
    ui_drawScrollbar(textOffset, (uint8_t)(infoLineCount - visible + 1));
  }
}

uint8_t ui_wifiQrScrollMax() {
  static const uint8_t infoLineCount = 5;
  const uint8_t visible = WIFI_QR_TEXT_VISIBLE;
  uint8_t textPages = 1;
  if (infoLineCount > visible) {
    textPages = (uint8_t)(infoLineCount - visible + 1);
  }
  return (uint8_t)(1 + textPages);
}
