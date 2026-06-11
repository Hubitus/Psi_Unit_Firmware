/*
 * storage.h — user settings persistence in non-volatile memory (NVS)
 *
 * Settings (modes, setpoints, display brightness, etc.) are stored in ESP32 flash.
 * load/save read and write SavedSettings with CRC verification.
 */

#ifndef STORAGE_H
#define STORAGE_H

#include "config.h"
#include "types.h"

extern SavedSettings settings;

void storage_begin();
// true = NVS (Preferences) open; settings can be read/saved
bool storage_isReady();

uint16_t storage_loadDstAppliedKey();
void storage_saveDstAppliedKey(uint16_t key);

uint16_t storage_calculateCRC16(const uint8_t *data, uint16_t length);
void storage_loadDefaults();
bool storage_save();
void storage_requestSave();
void storage_pollSave();
bool storage_load();
void storage_logLoadFailure();
void storage_logSaveFailure();

#endif // STORAGE_H
