/*
 * settings_access.h — unified interface for reading any settings field
 *
 * Combines SavedSettings (settings_field) and runtime fields (Wi-Fi, log)
 * for the menu and web interface.
 */

#ifndef SETTINGS_ACCESS_H
#define SETTINGS_ACCESS_H

#include "types.h"

uint8_t settings_accessGetUInt8(SettingsFieldId field);
int16_t settings_accessGetInt16(SettingsFieldId field);

#endif // SETTINGS_ACCESS_H
