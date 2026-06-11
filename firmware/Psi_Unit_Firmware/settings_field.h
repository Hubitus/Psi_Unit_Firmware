/*
 * settings_field.h — read and write SavedSettings structure fields
 *
 * Access by SettingsFieldId instead of direct settings.* references.
 * Used by the menu, app_commands, and wifi_web when changing NVS settings.
 */

#ifndef SETTINGS_FIELD_H
#define SETTINGS_FIELD_H

#include "types.h"

uint8_t settings_fieldGetUInt8(SettingsFieldId field);
void settings_fieldSetUInt8(SettingsFieldId field, uint8_t value);
int16_t settings_fieldGetInt16(SettingsFieldId field);
void settings_fieldSetInt16(SettingsFieldId field, int16_t value);

bool settings_fieldIsTempDeci(SettingsFieldId field);
bool settings_fieldIsBrightnessPercent(SettingsFieldId field);
bool settings_fieldAffectsSystemDisplay(SettingsFieldId field);
uint8_t settings_fieldEncoderStep(SettingsFieldId field);
TimeUnit settings_fieldCycleUnit(SettingsFieldId field);
uint8_t settings_fieldBrightnessRawToPercent(uint8_t raw, SettingsFieldId field);
uint8_t settings_fieldBrightnessPercentToRaw(uint8_t percent, SettingsFieldId field);

// Clamp/validate a proposed value; returns false if out of allowed range.
bool settings_fieldValidateValue(SettingsFieldId field, ItemType itemType, int16_t inValue, int16_t &outValue);

#endif // SETTINGS_FIELD_H
