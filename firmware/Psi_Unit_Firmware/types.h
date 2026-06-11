/*
 * types.h — shared firmware data types
 *
 * Output modes, menu screen states, persisted settings structure,
 * and menu item descriptors. Used by UI, logic, and storage modules.
 */

#ifndef TYPES_H
#define TYPES_H

#include <Arduino.h>

// Output mode: AUTO, forced on/off, or MANUAL cycle (humidifier/fan).
enum Mode : uint8_t { MODE_AUTO = 0, MODE_FORCE_ON = 1, MODE_FORCE_OFF = 2, MODE_MANUAL = 3 };
// Time unit for work/rest cycles: seconds or minutes.
enum TimeUnit : uint8_t { UNIT_SECONDS = 0, UNIT_MINUTES = 1 };
// Current OLED screen (menu navigation).
enum UiState : uint8_t {
  UI_MAIN,           // main screen with sensors and outputs
  UI_MAIN_MENU,      // section grid INC / GRH / … / BACK
  UI_SUB_MENU,       // parameter list for selected section
  UI_EDIT,           // numeric or mode editing
  UI_CLOCK_EDIT,     // RTC date and time setup
  UI_CONFIRM_RESET,  // settings reset confirmation
  UI_SYSTEM_INFO,    // scrollable diagnostics
  UI_WIFI_QR         // QR code and Wi‑Fi details
};
// Menu item type: mode, number, submenu, QR, reset, etc.
enum ItemType : uint8_t {
  IT_MODE, IT_INT, IT_HYST, IT_UNIT, IT_CLOCK_SET, IT_BACK, IT_CLEAR, IT_BOOL, IT_SUBMENU,
  IT_WIFI_QR, IT_SAVE
};
// Main menu sections (INC, GRH, HUM, FAN, LGT, SET, BACK).
enum MainMenuItem : uint8_t { MM_INC, MM_HEAT, MM_HUM, MM_FAN, MM_LIGHT, MM_SETTINGS, MM_EXIT, MAIN_MENU_COUNT };

// Item order in the SET section (settingsItems[]).
enum SettingsMenuIndex : uint8_t {
  SM_SET_WIFI = 0,
  SM_SET_RGB = 1,
  SM_SET_DISPLAY = 2,
  SM_SET_CLOCK = 3,
#if SENSOR_LOG_ENABLE
  SM_SET_LOG = 4,
  SM_SET_SYSINFO = 5,
  SM_SET_RESET = 6,
  SM_SET_BACK = 7
#else
  SM_SET_SYSINFO = 4,
  SM_SET_RESET = 5,
  SM_SET_BACK = 6
#endif
};

enum SettingsFieldId : uint8_t {
  SF_NONE = 0,
  SF_HEAT_MODE,
  SF_T_TARGET,
  SF_T_HYST,
  SF_HUM_MODE,
  SF_H_TARGET,
  SF_H_HYST,
  SF_HUM_MAX,
  SF_HUM_WORK,
  SF_HUM_REST,
  SF_HUM_WORK_UNIT,
  SF_HUM_REST_UNIT,
  SF_FAN_MODE,
  SF_FAN_WORK,
  SF_FAN_REST,
  SF_FAN_WORK_UNIT,
  SF_FAN_REST_UNIT,
  SF_LIGHT_MODE,
  SF_LIGHT_HOUR,
  SF_LIGHT_DURATION,
  SF_LIGHT_BRIGHTNESS,
  SF_INC_MODE,
  SF_INC_TARGET,
  SF_INC_HYST,
  SF_SLEEP_MIN,
  SF_BRIGHTNESS,
  SF_DISPLAY_ROTATED,
  SF_DISPLAY_INVERTED,
  SF_RGB_LED_ENABLED,
  SF_RGB_BRIGHTNESS,
  SF_WIFI_TIMEOUT,
  SF_WIFI_AP_ENABLED,
  SF_LOG_ENABLED,
  SF_LOG_INTERVAL
};

// All user settings stored in NVS (with CRC; see storage.cpp).
#pragma pack(push, 1)
struct SavedSettings {
  uint16_t signature;
  uint8_t version;
  uint8_t heatMode, humMode, fanMode, lightMode, incMode;
  uint8_t lightDuration;
  int16_t tTarget, incTarget;
  uint8_t tHyst, incHyst, hTarget, hHyst;
  uint8_t fanWork, fanRest, fanWorkUnit, fanRestUnit;
  uint8_t humMax, humWork, humRest, humWorkUnit, humRestUnit;
  uint8_t lightHour, lightBrightness, brightness, sleepMin;
  uint8_t displayRotated;
  uint8_t displayInverted;
  uint8_t rgbLedEnabled;
  uint8_t rgbBrightness;
  uint16_t crc16;
};
#pragma pack(pop)

struct MenuItem {
  const char *label;
  ItemType type;
  SettingsFieldId field;
  int16_t minv, maxv;
};

struct SensorState {
  bool active;
  unsigned long lastValidRead;
  uint8_t errorCount;
  bool dataValid;
};

#endif // TYPES_H
