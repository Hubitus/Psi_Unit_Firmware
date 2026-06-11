/*
 * display_ui.h — display and menu (renderer via display_backend)
 */

#ifndef DISPLAY_UI_H
#define DISPLAY_UI_H

#include "config.h"
#include "types.h"
#include "app_snapshot.h"
#include "display_backend.h"

extern UiState uiState;
extern int8_t mainMenuIndex;
extern MainMenuItem currentSection;
extern int8_t subMenuIndex;
extern int8_t subMenuOffset;
extern MenuItem currentItem;
extern int16_t editValue;
extern int16_t editBackup;
extern bool clockEditingField;
extern int8_t currentClockField;
extern int16_t clockEditValue;

void ui_requestRedraw();
void ui_requestRedrawIfMainVisible();
bool ui_needsRedraw();
void ui_clearRedrawFlag();

void ui_toastShow(unsigned long nowMillis);
bool ui_toastIsActive(unsigned long nowMillis);
bool ui_toastPollExpiry(unsigned long nowMillis);
void ui_drawToastOverlayIfActive(unsigned long nowMillis);
bool ui_isOledBusBusy();
void ui_setOledBusBusy(bool busy);

uint8_t ui_getSubMenuCount(MainMenuItem section);
const MenuItem* ui_getSectionItems(MainMenuItem section);

void ui_clockEditBegin();
void ui_clockEditCancel();
bool ui_clockEditOnShortPress();
bool ui_clockEditWasSaved();
bool ui_clockEditOnLongPress();
void ui_clockEditOnEncoder(int delta);

bool ui_isSettingsDisplayMenuActive();
void ui_openSettingsDisplayMenu();
void ui_closeSettingsDisplayMenu();
bool ui_isSettingsRgbMenuActive();
void ui_openSettingsRgbMenu();
void ui_closeSettingsRgbMenu();
bool ui_isSettingsWifiMenuActive();
void ui_openSettingsWifiMenu();
void ui_closeSettingsWifiMenu();
bool ui_isSettingsLogMenuActive();
void ui_openSettingsLogMenu();
void ui_closeSettingsLogMenu();
void ui_logMenuApplyDrafts();

enum ClockEditField : int8_t {
  CLOCK_FIELD_YEAR = 0,
  CLOCK_FIELD_MONTH,
  CLOCK_FIELD_DAY,
  CLOCK_FIELD_HOUR,
  CLOCK_FIELD_MINUTE,
  CLOCK_FIELD_SAVE,
  CLOCK_FIELD_BACK,
  CLOCK_FIELD_COUNT
};

void ui_sleepDisplay();
void ui_wakeUp();
const char* ui_getModeShort(Mode mode);
void ui_loadEdit();
bool ui_applyEdit();
void ui_applyDisplaySettings();
void ui_applyOledContrast(uint8_t rawContrast);
void ui_applyOledTheme();
void ui_resetSettingsToDefaults();
void ui_formatValue(char* buf, MenuItem &item);

void ui_drawSplash();
void ui_drawMain(const AppSnapshot &snap);
void ui_drawMainMenu();
void ui_drawSubMenu();
void ui_drawEdit();
void ui_drawResetConfirm();
void ui_drawClockEdit();
uint8_t ui_sysinfoDisplayCount();
void ui_drawSystemInfo();
void ui_drawWifiQr();
void ui_wifiQrBegin();
void ui_wifiQrOnEncoder(int delta);
uint8_t ui_wifiQrScrollMax();

#endif // DISPLAY_UI_H
