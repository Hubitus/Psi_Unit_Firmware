/*
 * app_commands.h — single entry point for settings and state changes
 *
 * All changes (display, web, Serial) go through app_dispatch: set field, reset
 * settings, clock, sensor log, light toggle. Returns which side effects to
 * apply (redraw, RGB, system_info).
 */

#ifndef APP_COMMANDS_H
#define APP_COMMANDS_H

#include "types.h"

enum AppCommand : uint8_t {
  CMD_NONE = 0,
  CMD_SET_FIELD,
  CMD_RESET_SETTINGS,
  CMD_TOGGLE_LIGHT_OVERRIDE,
  CMD_SET_RTC,
  CMD_COMMIT_RTC,
  CMD_SET_LOG,
  CMD_CLEAR_SENSOR_LOG,
};

struct AppSetFieldPayload {
  SettingsFieldId field;
  ItemType itemType;
  int16_t value;
};

struct AppSetRtcPayload {
  uint16_t year;
  uint8_t month;
  uint8_t day;
  uint8_t hour;
  uint8_t minute;
  uint8_t second;
};

struct AppSetLogPayload {
  bool hasEnabled;
  bool enabled;
  bool hasInterval;
  uint8_t intervalMin;
};

struct AppCommandEffects {
  bool applyPersistedSettings;
  bool applyRgbSettings;
  bool redrawMainIfVisible;
  bool markMainDirty;
  bool refreshSystemInfo;
};

struct AppCommandResult {
  bool ok;
  AppCommandEffects effects;
};

AppCommandResult app_dispatch(AppCommand cmd, const void *payload);

#endif // APP_COMMANDS_H
