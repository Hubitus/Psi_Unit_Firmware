/*
 * settings_field.cpp — get/set individual SavedSettings fields by SettingsFieldId
 */

#include "settings_field.h"
#include "storage.h"
#include "config.h"

uint8_t settings_fieldGetUInt8(SettingsFieldId field) {
  switch (field) {
    case SF_HEAT_MODE: return settings.heatMode;
    case SF_HUM_MODE: return settings.humMode;
    case SF_FAN_MODE: return settings.fanMode;
    case SF_LIGHT_MODE: return settings.lightMode;
    case SF_INC_MODE: return settings.incMode;
    case SF_T_HYST: return settings.tHyst;
    case SF_INC_HYST: return settings.incHyst;
    case SF_H_TARGET: return settings.hTarget;
    case SF_H_HYST: return settings.hHyst;
    case SF_HUM_MAX: return settings.humMax;
    case SF_HUM_WORK: return settings.humWork;
    case SF_HUM_REST: return settings.humRest;
    case SF_HUM_WORK_UNIT: return settings.humWorkUnit;
    case SF_HUM_REST_UNIT: return settings.humRestUnit;
    case SF_FAN_WORK: return settings.fanWork;
    case SF_FAN_REST: return settings.fanRest;
    case SF_FAN_WORK_UNIT: return settings.fanWorkUnit;
    case SF_FAN_REST_UNIT: return settings.fanRestUnit;
    case SF_LIGHT_HOUR: return settings.lightHour;
    case SF_LIGHT_DURATION: return settings.lightDuration;
    case SF_LIGHT_BRIGHTNESS: return settings.lightBrightness;
    case SF_SLEEP_MIN: return settings.sleepMin;
    case SF_BRIGHTNESS: return settings.brightness;
    case SF_DISPLAY_ROTATED: return settings.displayRotated;
    case SF_DISPLAY_INVERTED: return settings.displayInverted;
    case SF_RGB_LED_ENABLED: return settings.rgbLedEnabled;
    case SF_RGB_BRIGHTNESS: return settings.rgbBrightness;
    default: return 0;
  }
}

void settings_fieldSetUInt8(SettingsFieldId field, uint8_t value) {
  switch (field) {
    case SF_HEAT_MODE: settings.heatMode = value; break;
    case SF_HUM_MODE: settings.humMode = value; break;
    case SF_FAN_MODE: settings.fanMode = value; break;
    case SF_LIGHT_MODE: settings.lightMode = value; break;
    case SF_INC_MODE: settings.incMode = value; break;
    case SF_T_HYST: settings.tHyst = value; break;
    case SF_INC_HYST: settings.incHyst = value; break;
    case SF_H_TARGET: settings.hTarget = value; break;
    case SF_H_HYST: settings.hHyst = value; break;
    case SF_HUM_MAX: settings.humMax = value; break;
    case SF_HUM_WORK: settings.humWork = value; break;
    case SF_HUM_REST: settings.humRest = value; break;
    case SF_HUM_WORK_UNIT: settings.humWorkUnit = value; break;
    case SF_HUM_REST_UNIT: settings.humRestUnit = value; break;
    case SF_FAN_WORK: settings.fanWork = value; break;
    case SF_FAN_REST: settings.fanRest = value; break;
    case SF_FAN_WORK_UNIT: settings.fanWorkUnit = value; break;
    case SF_FAN_REST_UNIT: settings.fanRestUnit = value; break;
    case SF_LIGHT_HOUR: settings.lightHour = value; break;
    case SF_LIGHT_DURATION: settings.lightDuration = value; break;
    case SF_LIGHT_BRIGHTNESS:
      if (value > 100U) {
        value = 100U;
      }
      settings.lightBrightness = (uint8_t)(((value + 5U) / 10U) * 10U);
      break;
    case SF_SLEEP_MIN: settings.sleepMin = value; break;
    case SF_BRIGHTNESS: settings.brightness = value; break;
    case SF_DISPLAY_ROTATED: settings.displayRotated = value; break;
    case SF_DISPLAY_INVERTED: settings.displayInverted = value; break;
    case SF_RGB_LED_ENABLED: settings.rgbLedEnabled = value; break;
    case SF_RGB_BRIGHTNESS: settings.rgbBrightness = value; break;
    default: break;
  }
}

int16_t settings_fieldGetInt16(SettingsFieldId field) {
  switch (field) {
    case SF_T_TARGET: return settings.tTarget;
    case SF_INC_TARGET: return settings.incTarget;
    default: return 0;
  }
}

void settings_fieldSetInt16(SettingsFieldId field, int16_t value) {
  switch (field) {
    case SF_T_TARGET: settings.tTarget = value; break;
    case SF_INC_TARGET: settings.incTarget = value; break;
    default: break;
  }
}

bool settings_fieldIsTempDeci(SettingsFieldId field) {
  return field == SF_T_TARGET || field == SF_INC_TARGET;
}

bool settings_fieldIsBrightnessPercent(SettingsFieldId field) {
  return field == SF_BRIGHTNESS || field == SF_RGB_BRIGHTNESS;
}

static uint8_t settings_brightnessStepPercent(SettingsFieldId field) {
  return (field == SF_RGB_BRIGHTNESS) ? 10U : 5U;
}

bool settings_fieldAffectsSystemDisplay(SettingsFieldId field) {
  return field == SF_SLEEP_MIN || field == SF_BRIGHTNESS ||
         field == SF_DISPLAY_ROTATED || field == SF_DISPLAY_INVERTED;
}

uint8_t settings_fieldEncoderStep(SettingsFieldId field) {
  if (field == SF_BRIGHTNESS) {
    return 5;
  }
  if (field == SF_RGB_BRIGHTNESS || field == SF_LIGHT_BRIGHTNESS) {
    return 10;
  }
  return 1;
}

TimeUnit settings_fieldCycleUnit(SettingsFieldId field) {
  if (field == SF_HUM_WORK) {
    return (TimeUnit)settings.humWorkUnit;
  }
  if (field == SF_HUM_REST) {
    return (TimeUnit)settings.humRestUnit;
  }
  if (field == SF_FAN_WORK) {
    return (TimeUnit)settings.fanWorkUnit;
  }
  if (field == SF_FAN_REST) {
    return (TimeUnit)settings.fanRestUnit;
  }
  return UNIT_MINUTES;
}

uint8_t settings_fieldBrightnessRawToPercent(uint8_t raw, SettingsFieldId field) {
  const uint8_t step = settings_brightnessStepPercent(field);
  uint16_t percent = ((uint16_t)raw * 100U + 127U) / 255U;
  percent = ((percent + step / 2U) / step) * step;
  if (percent > 100U) {
    percent = 100U;
  }
  return (uint8_t)percent;
}

uint8_t settings_fieldBrightnessPercentToRaw(uint8_t percent, SettingsFieldId field) {
  if (percent > 100U) {
    percent = 100U;
  }
  const uint8_t step = settings_brightnessStepPercent(field);
  percent = ((percent + step / 2U) / step) * step;
  return (uint8_t)(((uint16_t)percent * 255U + 50U) / 100U);
}

static bool settings_fieldInRange(int16_t value, int16_t minv, int16_t maxv) {
  return value >= minv && value <= maxv;
}

bool settings_fieldValidateValue(SettingsFieldId field, ItemType itemType, int16_t inValue, int16_t &outValue) {
  outValue = inValue;

  switch (itemType) {
    case IT_MODE: {
      const int16_t maxMode = (field == SF_HUM_MODE) ? MODE_MANUAL : MODE_FORCE_OFF;
      if (!settings_fieldInRange(inValue, MODE_AUTO, maxMode)) {
        return false;
      }
      return true;
    }
    case IT_BOOL:
      outValue = (inValue != 0) ? 1 : 0;
      return true;
    case IT_UNIT:
      return settings_fieldInRange(inValue, UNIT_SECONDS, UNIT_MINUTES);
    case IT_HYST:
      if (field == SF_T_HYST || field == SF_INC_HYST) {
        return settings_fieldInRange(inValue, TEMP_HYST_MIN, TEMP_HYST_MAX);
      }
      if (field == SF_H_HYST) {
        return settings_fieldInRange(inValue, 1, 20);
      }
      return false;
    case IT_INT:
      break;
    default:
      return false;
  }

  if (settings_fieldIsTempDeci(field)) {
    return settings_fieldInRange(inValue, TEMP_SET_MIN, TEMP_SET_MAX);
  }
  if (field == SF_H_TARGET) {
    return settings_fieldInRange(inValue, HUM_SET_MIN, HUM_SET_MAX);
  }
  if (field == SF_HUM_MAX || field == SF_HUM_WORK || field == SF_HUM_REST || field == SF_FAN_WORK ||
      field == SF_FAN_REST) {
    return settings_fieldInRange(inValue, 1, 250);
  }
  if (field == SF_LIGHT_HOUR) {
    return settings_fieldInRange(inValue, 0, 23);
  }
  if (field == SF_LIGHT_DURATION) {
    return settings_fieldInRange(inValue, 1, 24);
  }
  if (field == SF_LIGHT_BRIGHTNESS) {
    if (!settings_fieldInRange(inValue, 0, 100)) {
      return false;
    }
    outValue = (int16_t)((((int)inValue + 5) / 10) * 10);
    return true;
  }
  if (field == SF_SLEEP_MIN) {
    return settings_fieldInRange(inValue, 0, 60);
  }
  if (settings_fieldIsBrightnessPercent(field)) {
    if (!settings_fieldInRange(inValue, 0, 100)) {
      return false;
    }
    const uint8_t step = settings_brightnessStepPercent(field);
    outValue = (int16_t)((((int)inValue + (int)step / 2) / (int)step) * (int)step);
    return true;
  }
  if (field == SF_DISPLAY_ROTATED || field == SF_DISPLAY_INVERTED || field == SF_RGB_LED_ENABLED) {
    return settings_fieldInRange(inValue, 0, 1);
  }
  return false;
}
