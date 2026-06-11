/*
 * app_commands.cpp — execute settings change commands
 *
 * Validates values, writes to NVS or runtime stores (Wi-Fi, log), and builds
 * the side-effect list for app_effects_apply().
 */

#include "app_commands.h"
#include "storage.h"
#include "settings_field.h"
#include "logic.h"
#include "app_snapshot.h"
#include "wifi_manager.h"
#include "sensor_log.h"
#include "rtc_clock.h"
#include "config.h"

static AppCommandEffects app_effectsNone() {
  return AppCommandEffects{};
}

static AppCommandResult app_result(bool ok, AppCommandEffects effects = app_effectsNone()) {
  return AppCommandResult{ok, effects};
}

static uint8_t app_logIntervalSnap(uint8_t minutes) {
  if (minutes <= 7) {
    return 5;
  }
  if (minutes <= 12) {
    return 10;
  }
  return 15;
}

static bool app_cmdSetField(const AppSetFieldPayload &payload, AppCommandEffects &fx) {
  if (payload.field == SF_NONE) {
    return false;
  }

  if (payload.field == SF_LOG_ENABLED || payload.field == SF_LOG_INTERVAL) {
    return false;
  }

  if (payload.field == SF_WIFI_AP_ENABLED) {
    const bool enable = (payload.value != 0);
    if (enable) {
      wifi_manager_setUserApDisabled(false);
      if (!wifi_manager_startSession()) {
        return false;
      }
    } else {
      wifi_manager_setUserApDisabled(true);
      wifi_manager_stopSession();
    }
    fx.applyRgbSettings = true;
    fx.refreshSystemInfo = true;
    fx.redrawMainIfVisible = true;
    return true;
  }

  if (payload.field == SF_WIFI_TIMEOUT) {
    return wifi_manager_setSessionTimeoutMin((uint8_t)payload.value);
  }

  switch (payload.itemType) {
    case IT_MODE:
    case IT_INT:
    case IT_HYST:
    case IT_UNIT:
    case IT_BOOL:
      break;
    default:
      return false;
  }

  int16_t validated = payload.value;
  if (!settings_fieldValidateValue(payload.field, payload.itemType, payload.value, validated)) {
    return false;
  }

  if (settings_fieldIsTempDeci(payload.field)) {
    settings_fieldSetInt16(payload.field, validated);
  } else if (settings_fieldIsBrightnessPercent(payload.field)) {
    settings_fieldSetUInt8(payload.field, settings_fieldBrightnessPercentToRaw((uint8_t)validated, payload.field));
  } else if (payload.itemType == IT_BOOL) {
    settings_fieldSetUInt8(payload.field, (validated == 1) ? 1 : 0);
  } else if (payload.field == SF_LIGHT_BRIGHTNESS) {
    settings_fieldSetUInt8(payload.field, (uint8_t)validated);
  } else {
    settings_fieldSetUInt8(payload.field, (uint8_t)validated);
  }

  if (settings_fieldAffectsSystemDisplay(payload.field) || payload.field == SF_RGB_LED_ENABLED ||
      payload.field == SF_RGB_BRIGHTNESS) {
    fx.applyPersistedSettings = true;
  }

  storage_requestSave();
  return true;
}

static bool app_cmdResetSettings(AppCommandEffects &fx) {
  storage_loadDefaults();
  storage_requestSave();
  fx.applyPersistedSettings = true;
  fx.markMainDirty = true;
  return true;
}

static bool app_cmdToggleLightOverride() {
  logic_toggleLightOverride();
  return true;
}

static bool app_cmdSetRtc(AppCommandEffects &fx) {
  rtc_normalizeDraft();
  const bool ok = rtc_commitClockEdit();
  if (ok) {
    fx.markMainDirty = true;
  }
  return ok;
}

static bool app_cmdSetLog(const AppSetLogPayload &payload) {
#if SENSOR_LOG_ENABLE
  if (!sensor_log_isCompiledIn()) {
    return false;
  }
  if (payload.hasEnabled) {
    sensor_log_setEnabled(payload.enabled);
  }
  if (payload.hasInterval) {
    return sensor_log_setIntervalMin(app_logIntervalSnap(payload.intervalMin));
  }
  return true;
#else
  (void)payload;
  return false;
#endif
}

static bool app_cmdClearSensorLog() {
#if SENSOR_LOG_ENABLE
  return sensor_log_isCompiledIn() && sensor_log_clear();
#else
  return false;
#endif
}

static bool app_cmdCommitRtc(AppCommandEffects &fx) {
  const bool ok = rtc_commitClockEdit();
  if (ok) {
    fx.markMainDirty = true;
  }
  return ok;
}

// Route command to the matching handler.
AppCommandResult app_dispatch(AppCommand cmd, const void *payload) {
  AppCommandEffects fx = app_effectsNone();

  switch (cmd) {
    case CMD_SET_FIELD: {
      if (payload == nullptr) {
        return app_result(false);
      }
      const bool ok = app_cmdSetField(*static_cast<const AppSetFieldPayload *>(payload), fx);
      return app_result(ok, fx);
    }
    case CMD_RESET_SETTINGS:
      return app_result(app_cmdResetSettings(fx), fx);
    case CMD_TOGGLE_LIGHT_OVERRIDE:
      return app_result(app_cmdToggleLightOverride());
    case CMD_SET_RTC: {
      if (payload == nullptr) {
        return app_result(false);
      }
      const AppSetRtcPayload &rtcPayload = *static_cast<const AppSetRtcPayload *>(payload);
      rtcEditDraft = DateTime((uint16_t)rtcPayload.year, rtcPayload.month, rtcPayload.day, rtcPayload.hour,
                              rtcPayload.minute, rtcPayload.second);
      return app_result(app_cmdSetRtc(fx), fx);
    }
    case CMD_COMMIT_RTC:
      return app_result(app_cmdCommitRtc(fx), fx);
    case CMD_SET_LOG: {
      if (payload == nullptr) {
        return app_result(false);
      }
      return app_result(app_cmdSetLog(*static_cast<const AppSetLogPayload *>(payload)));
    }
    case CMD_CLEAR_SENSOR_LOG:
      return app_result(app_cmdClearSensorLog());
    default:
      return app_result(false);
  }
}
