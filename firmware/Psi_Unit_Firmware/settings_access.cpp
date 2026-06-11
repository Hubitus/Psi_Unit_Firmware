/*
 * settings_access.cpp — read routing: NVS or runtime by field type
 */

#include "settings_access.h"
#include "settings_field.h"
#include "settings_runtime.h"

uint8_t settings_accessGetUInt8(SettingsFieldId field) {
  if (settings_runtimeIsExternalField(field)) {
    return settings_runtimeGetUInt8(field);
  }
  return settings_fieldGetUInt8(field);
}

int16_t settings_accessGetInt16(SettingsFieldId field) {
  return settings_fieldGetInt16(field);
}
