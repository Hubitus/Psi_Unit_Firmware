/*
 * settings_runtime.h — settings outside SavedSettings (Wi-Fi, sensor log)
 *
 * Read-only; changes go through app_commands (separate NVS stores).
 */

#ifndef SETTINGS_RUNTIME_H
#define SETTINGS_RUNTIME_H

#include "types.h"

bool settings_runtimeIsExternalField(SettingsFieldId field);
uint8_t settings_runtimeGetUInt8(SettingsFieldId field);

#endif // SETTINGS_RUNTIME_H
