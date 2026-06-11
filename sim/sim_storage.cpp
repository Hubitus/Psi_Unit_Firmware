/*
 * sim_storage.cpp — RAM-backed settings for desktop simulator
 */

#include "storage.h"
#include "config.h"
#include <string.h>

SavedSettings settings;
static bool storageReady = true;

void storage_begin() {
  storageReady = true;
}

bool storage_isReady() {
  return storageReady;
}

uint16_t storage_loadDstAppliedKey() {
  return 0;
}

void storage_saveDstAppliedKey(uint16_t) {}

uint16_t storage_calculateCRC16(const uint8_t *data, uint16_t length) {
  uint16_t crc = 0xFFFF;
  for (uint16_t i = 0; i < length; i++) {
    crc ^= data[i];
    for (uint8_t j = 0; j < 8; j++) {
      if (crc & 1) {
        crc = (uint16_t)((crc >> 1) ^ 0xA001);
      } else {
        crc >>= 1;
      }
    }
  }
  return crc;
}

static void storage_refreshSettingsCrc() {
  settings.crc16 = storage_calculateCRC16((const uint8_t *)&settings, sizeof(SavedSettings) - 2);
}

void storage_loadDefaults() {
  settings.signature = SETTINGS_SIG;
  settings.version = SETTINGS_VER;
  settings.heatMode = MODE_FORCE_OFF;
  settings.humMode = MODE_FORCE_OFF;
  settings.fanMode = MODE_FORCE_OFF;
  settings.lightMode = MODE_FORCE_OFF;
  settings.incMode = MODE_FORCE_OFF;
  settings.tTarget = 250;
  settings.incTarget = 250;
  settings.tHyst = 10;
  settings.incHyst = 10;
  settings.hTarget = 60;
  settings.hHyst = 5;
  settings.humMax = 10;
  settings.fanWork = 15;
  settings.fanRest = 10;
  settings.fanWorkUnit = UNIT_SECONDS;
  settings.fanRestUnit = UNIT_MINUTES;
  settings.humWork = 5;
  settings.humRest = 25;
  settings.humWorkUnit = UNIT_MINUTES;
  settings.humRestUnit = UNIT_MINUTES;
  settings.lightHour = 8;
  settings.lightDuration = 12;
  settings.lightBrightness = 100;
  settings.brightness = 128;
  settings.sleepMin = 2;
  settings.displayRotated = 0;
  settings.displayInverted = 0;
  settings.rgbLedEnabled = 1;
  settings.rgbBrightness = 255;
  storage_refreshSettingsCrc();
}

bool storage_save() {
  storage_refreshSettingsCrc();
  return storageReady;
}

void storage_requestSave() {}

void storage_pollSave() {}

bool storage_load() {
  storage_loadDefaults();
  return true;
}

void storage_logLoadFailure() {}

void storage_logSaveFailure() {}
