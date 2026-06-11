/*
 * storage.cpp — settings read/write in flash (NVS / Preferences)
 *
 * Two redundant banks with CRC protect settings from corruption on power loss.
 * Legacy v21/v22 formats load with automatic migration to SETTINGS_VER from config.h.
 */

#include "storage.h"
#include <Arduino.h>

#if defined(ARDUINO_ARCH_ESP32)
#include <Preferences.h>
#else
#error "Psi Unit targets Arduino Nano ESP32 (Preferences/NVS storage)."
#endif

SavedSettings settings;

static const uint16_t STORAGE_META_MAGIC = 0xA5C5;

#pragma pack(push, 1)
struct StorageMeta {
  uint16_t magic;
  uint8_t activeBank;
  uint8_t seq;
  uint16_t crc16;
};
#pragma pack(pop)

static Preferences prefs;
static bool storageReady = false;

// Settings format v21 (legacy: single fanUnit for work and rest).
#pragma pack(push, 1)
struct SavedSettingsV21 {
  uint16_t signature;
  uint8_t version;
  uint8_t heatMode, humMode, fanMode, lightMode, incMode;
  uint8_t lightDuration;
  int16_t tTarget, incTarget;
  uint8_t tHyst, incHyst, hTarget, hHyst;
  uint8_t fanWork, fanRest, fanUnit;
  uint8_t humMax, humWork, humRest, humUnit;
  uint8_t lightHour, lightBrightness, brightness, sleepMin;
  uint8_t humFanEnabled;
  uint8_t displayRotated;
  uint8_t displayInverted;
  uint8_t rgbLedEnabled;
  uint16_t crc16;
};
#pragma pack(pop)

// Settings format v24 (single humUnit for work and rest).
#pragma pack(push, 1)
struct SavedSettingsV24 {
  uint16_t signature;
  uint8_t version;
  uint8_t heatMode, humMode, fanMode, lightMode, incMode;
  uint8_t lightDuration;
  int16_t tTarget, incTarget;
  uint8_t tHyst, incHyst, hTarget, hHyst;
  uint8_t fanWork, fanRest, fanWorkUnit, fanRestUnit;
  uint8_t humMax, humWork, humRest, humUnit;
  uint8_t lightHour, lightBrightness, brightness, sleepMin;
  uint8_t displayRotated;
  uint8_t displayInverted;
  uint8_t rgbLedEnabled;
  uint8_t rgbBrightness;
  uint16_t crc16;
};
#pragma pack(pop)

// Settings format v23 (before rgbBrightness field).
#pragma pack(push, 1)
struct SavedSettingsV23 {
  uint16_t signature;
  uint8_t version;
  uint8_t heatMode, humMode, fanMode, lightMode, incMode;
  uint8_t lightDuration;
  int16_t tTarget, incTarget;
  uint8_t tHyst, incHyst, hTarget, hHyst;
  uint8_t fanWork, fanRest, fanWorkUnit, fanRestUnit;
  uint8_t humMax, humWork, humRest, humUnit;
  uint8_t lightHour, lightBrightness, brightness, sleepMin;
  uint8_t displayRotated;
  uint8_t displayInverted;
  uint8_t rgbLedEnabled;
  uint16_t crc16;
};
#pragma pack(pop)

// Settings format v22 (separate time units for fan work/rest).
#pragma pack(push, 1)
struct SavedSettingsV22 {
  uint16_t signature;
  uint8_t version;
  uint8_t heatMode, humMode, fanMode, lightMode, incMode;
  uint8_t lightDuration;
  int16_t tTarget, incTarget;
  uint8_t tHyst, incHyst, hTarget, hHyst;
  uint8_t fanWork, fanRest, fanWorkUnit, fanRestUnit;
  uint8_t humMax, humWork, humRest, humUnit;
  uint8_t lightHour, lightBrightness, brightness, sleepMin;
  uint8_t humFanEnabled;
  uint8_t displayRotated;
  uint8_t displayInverted;
  uint8_t rgbLedEnabled;
  uint16_t crc16;
};
#pragma pack(pop)

static bool savePending = false;
static unsigned long saveRequestedAt = 0;
static uint8_t lastSaveFailStep = 0;

// NVS key names (Preferences limit: 15 characters max).
static const char *NVS_NS = "psi_cfg";
static const char *KEY_META = "meta";
static const char *KEY_BANK_A = "bankA";
static const char *KEY_BANK_B = "bankB";
static const char *KEY_DST_APPLIED = "dstKey";

static const char *storage_bankKey(uint8_t bank) {
  return (bank == 0) ? KEY_BANK_A : KEY_BANK_B;
}

static uint16_t storage_metaCrc(const StorageMeta &meta) {
  return storage_calculateCRC16((const uint8_t *)&meta, sizeof(StorageMeta) - sizeof(uint16_t));
}

static void storage_refreshSettingsCrc() {
  settings.crc16 = storage_calculateCRC16((uint8_t *)&settings, sizeof(SavedSettings) - 2);
}

static bool storage_validateSettingsFields(
    uint8_t heatMode, uint8_t humMode, uint8_t fanMode, uint8_t lightMode, uint8_t incMode,
    int16_t tTarget, int16_t incTarget, uint8_t tHyst, uint8_t incHyst, uint8_t hTarget, uint8_t hHyst,
    uint8_t humMax, uint8_t humWork, uint8_t humRest, uint8_t humWorkUnit, uint8_t humRestUnit,
    uint8_t humUnitLegacy, bool splitHumUnits, uint8_t fanWork, uint8_t fanRest, uint8_t fanWorkUnit,
    uint8_t fanRestUnit, uint8_t fanUnitLegacy, bool splitFanUnits,
    uint8_t lightHour, uint8_t lightDuration, uint8_t lightBrightness, uint8_t sleepMin,
    uint8_t displayRotated, uint8_t displayInverted, uint8_t rgbLedEnabled) {
  bool valid = true;
  valid = valid && (heatMode <= MODE_MANUAL);
  valid = valid && (incMode <= MODE_MANUAL);
  valid = valid && (humMode <= MODE_MANUAL);
  valid = valid && (fanMode <= MODE_MANUAL);
  valid = valid && (lightMode <= MODE_MANUAL);
  valid = valid && (tTarget >= TEMP_SET_MIN && tTarget <= TEMP_SET_MAX);
  valid = valid && (incTarget >= TEMP_SET_MIN && incTarget <= TEMP_SET_MAX);
  valid = valid && (hTarget >= HUM_SET_MIN && hTarget <= HUM_SET_MAX);
  valid = valid && (tHyst >= TEMP_HYST_MIN && tHyst <= TEMP_HYST_MAX);
  valid = valid && (incHyst >= TEMP_HYST_MIN && incHyst <= TEMP_HYST_MAX);
  valid = valid && (hHyst >= 1 && hHyst <= 20);
  valid = valid && (humMax >= 1 && humMax <= 250);
  valid = valid && (fanWork >= 1 && fanWork <= 250);
  valid = valid && (fanRest >= 1 && fanRest <= 250);
  valid = valid && (humWork >= 1 && humWork <= 250);
  valid = valid && (humRest >= 1 && humRest <= 250);
  if (splitHumUnits) {
    valid = valid && (humWorkUnit <= UNIT_MINUTES);
    valid = valid && (humRestUnit <= UNIT_MINUTES);
  } else {
    valid = valid && (humUnitLegacy <= UNIT_MINUTES);
  }
  valid = valid && (lightHour <= 23);
  valid = valid && (lightDuration >= 1 && lightDuration <= 24);
  valid = valid && (lightBrightness <= 100 && (lightBrightness % 10) == 0);
  valid = valid && (sleepMin <= 60);
  valid = valid && (displayRotated <= 1);
  valid = valid && (displayInverted <= 1);
  valid = valid && (rgbLedEnabled <= 1);
  if (splitFanUnits) {
    valid = valid && (fanWorkUnit <= UNIT_MINUTES);
    valid = valid && (fanRestUnit <= UNIT_MINUTES);
  } else {
    valid = valid && (fanUnitLegacy <= UNIT_MINUTES);
  }
  return valid;
}

static bool storage_blockFieldsValid(const SavedSettings &s) {
  return storage_validateSettingsFields(
             s.heatMode, s.humMode, s.fanMode, s.lightMode, s.incMode, s.tTarget, s.incTarget, s.tHyst,
             s.incHyst, s.hTarget, s.hHyst, s.humMax, s.humWork, s.humRest, s.humWorkUnit, s.humRestUnit, 0,
             true, s.fanWork, s.fanRest, s.fanWorkUnit, s.fanRestUnit, 0, true, s.lightHour, s.lightDuration,
             s.lightBrightness, s.sleepMin, s.displayRotated, s.displayInverted, s.rgbLedEnabled) &&
         s.rgbBrightness <= 255;
}

static bool storage_blockCrcValid(const SavedSettings &block) {
  if (block.signature != SETTINGS_SIG || block.version != SETTINGS_VER) {
    return false;
  }
  const uint16_t calculated =
      storage_calculateCRC16((const uint8_t *)&block, sizeof(SavedSettings) - 2);
  return calculated == block.crc16;
}

static bool storage_blockFieldsValidV21(const SavedSettingsV21 &s) {
  return storage_validateSettingsFields(
      s.heatMode, s.humMode, s.fanMode, s.lightMode, s.incMode, s.tTarget, s.incTarget, s.tHyst,
      s.incHyst, s.hTarget, s.hHyst, s.humMax, s.humWork, s.humRest, s.humUnit, s.humUnit, 0, false,
      s.fanWork, s.fanRest, 0, 0, s.fanUnit, false, s.lightHour, s.lightDuration, s.lightBrightness,
      s.sleepMin,
      s.displayRotated, s.displayInverted, s.rgbLedEnabled) &&
         (s.humFanEnabled <= 1);
}

static bool storage_blockFieldsValidV23(const SavedSettingsV23 &s) {
  return storage_validateSettingsFields(
      s.heatMode, s.humMode, s.fanMode, s.lightMode, s.incMode, s.tTarget, s.incTarget, s.tHyst,
      s.incHyst, s.hTarget, s.hHyst, s.humMax, s.humWork, s.humRest, s.humUnit, s.humUnit, 0, false,
      s.fanWork, s.fanRest, s.fanWorkUnit, s.fanRestUnit, 0, true, s.lightHour, s.lightDuration,
      s.lightBrightness, s.sleepMin, s.displayRotated, s.displayInverted, s.rgbLedEnabled);
}

static bool storage_blockCrcValidV23(const SavedSettingsV23 &block) {
  if (block.signature != SETTINGS_SIG || block.version != 23) {
    return false;
  }
  const uint16_t calculated =
      storage_calculateCRC16((const uint8_t *)&block, sizeof(SavedSettingsV23) - 2);
  return calculated == block.crc16;
}

static bool storage_blockFieldsValidV22(const SavedSettingsV22 &s) {
  return storage_validateSettingsFields(
      s.heatMode, s.humMode, s.fanMode, s.lightMode, s.incMode, s.tTarget, s.incTarget, s.tHyst,
      s.incHyst, s.hTarget, s.hHyst, s.humMax, s.humWork, s.humRest, s.humUnit, s.humUnit, 0, false,
      s.fanWork, s.fanRest, s.fanWorkUnit, s.fanRestUnit, 0, true, s.lightHour, s.lightDuration,
      s.lightBrightness, s.sleepMin, s.displayRotated, s.displayInverted, s.rgbLedEnabled) &&
         (s.humFanEnabled <= 1);
}

static bool storage_blockCrcValidV21(const SavedSettingsV21 &block) {
  if (block.signature != SETTINGS_SIG || block.version != 21) {
    return false;
  }
  const uint16_t calculated =
      storage_calculateCRC16((const uint8_t *)&block, sizeof(SavedSettingsV21) - 2);
  return calculated == block.crc16;
}

static bool storage_blockCrcValidV22(const SavedSettingsV22 &block) {
  if (block.signature != SETTINGS_SIG || block.version != 22) {
    return false;
  }
  const uint16_t calculated =
      storage_calculateCRC16((const uint8_t *)&block, sizeof(SavedSettingsV22) - 2);
  return calculated == block.crc16;
}

static void storage_assignCommonFields(
    uint8_t heatMode, uint8_t humMode, uint8_t fanMode, uint8_t lightMode, uint8_t incMode,
    uint8_t lightDuration, int16_t tTarget, int16_t incTarget, uint8_t tHyst, uint8_t incHyst,
    uint8_t hTarget, uint8_t hHyst, uint8_t fanWork, uint8_t fanRest, uint8_t humMax, uint8_t humWork,
    uint8_t humRest, uint8_t humWorkUnit, uint8_t humRestUnit, uint8_t lightHour, uint8_t lightBrightness,
    uint8_t brightness,
    uint8_t sleepMin, uint8_t displayRotated, uint8_t displayInverted, uint8_t rgbLedEnabled) {
  settings.signature = SETTINGS_SIG;
  settings.version = SETTINGS_VER;
  settings.heatMode = heatMode;
  settings.humMode = humMode;
  settings.fanMode = fanMode;
  settings.lightMode = lightMode;
  settings.incMode = incMode;
  settings.lightDuration = lightDuration;
  settings.tTarget = tTarget;
  settings.incTarget = incTarget;
  settings.tHyst = tHyst;
  settings.incHyst = incHyst;
  settings.hTarget = hTarget;
  settings.hHyst = hHyst;
  settings.fanWork = fanWork;
  settings.fanRest = fanRest;
  settings.humMax = humMax;
  settings.humWork = humWork;
  settings.humRest = humRest;
  settings.humWorkUnit = humWorkUnit;
  settings.humRestUnit = humRestUnit;
  settings.lightHour = lightHour;
  settings.lightBrightness = lightBrightness;
  settings.brightness = brightness;
  settings.sleepMin = sleepMin;
  settings.displayRotated = displayRotated;
  settings.displayInverted = displayInverted;
  settings.rgbLedEnabled = rgbLedEnabled;
}

static bool storage_migrateV23ToCurrent(const SavedSettingsV23 &src) {
  storage_assignCommonFields(
      src.heatMode, src.humMode, src.fanMode, src.lightMode, src.incMode, src.lightDuration, src.tTarget,
      src.incTarget, src.tHyst, src.incHyst, src.hTarget, src.hHyst, src.fanWork, src.fanRest, src.humMax,
      src.humWork, src.humRest, src.humUnit, src.humUnit, src.lightHour, src.lightBrightness, src.brightness,
      src.sleepMin, src.displayRotated, src.displayInverted, src.rgbLedEnabled);
  settings.fanWorkUnit = src.fanWorkUnit;
  settings.fanRestUnit = src.fanRestUnit;
  settings.rgbBrightness = 255;
  storage_refreshSettingsCrc();
  return storage_blockFieldsValid(settings);
}

static bool storage_migrateV22ToCurrent(const SavedSettingsV22 &src) {
  storage_assignCommonFields(
      src.heatMode, src.humMode, src.fanMode, src.lightMode, src.incMode, src.lightDuration, src.tTarget,
      src.incTarget, src.tHyst, src.incHyst, src.hTarget, src.hHyst, src.fanWork, src.fanRest, src.humMax,
      src.humWork, src.humRest, src.humUnit, src.humUnit, src.lightHour, src.lightBrightness, src.brightness,
      src.sleepMin, src.displayRotated, src.displayInverted, src.rgbLedEnabled);
  settings.fanWorkUnit = src.fanWorkUnit;
  settings.fanRestUnit = src.fanRestUnit;
  settings.rgbBrightness = 255;
  storage_refreshSettingsCrc();
  return storage_blockFieldsValid(settings);
}

static bool storage_migrateV21ToCurrent(const SavedSettingsV21 &src) {
  storage_assignCommonFields(
      src.heatMode, src.humMode, src.fanMode, src.lightMode, src.incMode, src.lightDuration, src.tTarget,
      src.incTarget, src.tHyst, src.incHyst, src.hTarget, src.hHyst, src.fanWork, src.fanRest, src.humMax,
      src.humWork, src.humRest, src.humUnit, src.humUnit, src.lightHour, src.lightBrightness, src.brightness,
      src.sleepMin, src.displayRotated, src.displayInverted, src.rgbLedEnabled);
  settings.fanWorkUnit = src.fanUnit;
  settings.fanRestUnit = src.fanUnit;
  settings.rgbBrightness = 255;
  storage_refreshSettingsCrc();
  return storage_blockFieldsValid(settings);
}

static bool storage_blockCrcValidV24(const SavedSettingsV24 &block) {
  if (block.signature != SETTINGS_SIG || block.version != 24) {
    return false;
  }
  const uint16_t calculated =
      storage_calculateCRC16((const uint8_t *)&block, sizeof(SavedSettingsV24) - 2);
  return calculated == block.crc16;
}

static bool storage_blockFieldsValidV24(const SavedSettingsV24 &s) {
  return storage_validateSettingsFields(
      s.heatMode, s.humMode, s.fanMode, s.lightMode, s.incMode, s.tTarget, s.incTarget, s.tHyst,
      s.incHyst, s.hTarget, s.hHyst, s.humMax, s.humWork, s.humRest, s.humUnit, s.humUnit, 0, false,
      s.fanWork, s.fanRest, s.fanWorkUnit, s.fanRestUnit, 0, true, s.lightHour, s.lightDuration,
      s.lightBrightness, s.sleepMin, s.displayRotated, s.displayInverted, s.rgbLedEnabled);
}

static bool storage_migrateV24ToCurrent(const SavedSettingsV24 &src) {
  storage_assignCommonFields(
      src.heatMode, src.humMode, src.fanMode, src.lightMode, src.incMode, src.lightDuration, src.tTarget,
      src.incTarget, src.tHyst, src.incHyst, src.hTarget, src.hHyst, src.fanWork, src.fanRest, src.humMax,
      src.humWork, src.humRest, src.humUnit, src.humUnit, src.lightHour, src.lightBrightness, src.brightness,
      src.sleepMin, src.displayRotated, src.displayInverted, src.rgbLedEnabled);
  settings.fanWorkUnit = src.fanWorkUnit;
  settings.fanRestUnit = src.fanRestUnit;
  settings.rgbBrightness = src.rgbBrightness;
  storage_refreshSettingsCrc();
  return storage_blockFieldsValid(settings);
}

static bool storage_tryAcceptV24Bank(uint8_t bank) {
  SavedSettingsV24 block = {};
  const size_t bytesRead = prefs.getBytes(storage_bankKey(bank), &block, sizeof(block));
  if (bytesRead != sizeof(block)) {
    return false;
  }
  if (!storage_blockCrcValidV24(block) || !storage_blockFieldsValidV24(block)) {
    return false;
  }
  return storage_migrateV24ToCurrent(block);
}

static bool storage_tryAcceptV23Bank(uint8_t bank) {
  SavedSettingsV23 block = {};
  const size_t bytesRead = prefs.getBytes(storage_bankKey(bank), &block, sizeof(block));
  if (bytesRead != sizeof(block)) {
    return false;
  }
  if (!storage_blockCrcValidV23(block) || !storage_blockFieldsValidV23(block)) {
    return false;
  }
  return storage_migrateV23ToCurrent(block);
}

static void storage_sanitizeSettings() {
  if (settings.tHyst < TEMP_HYST_MIN) {
    settings.tHyst = TEMP_HYST_MIN;
  } else if (settings.tHyst > TEMP_HYST_MAX) {
    settings.tHyst = TEMP_HYST_MAX;
  }
  if (settings.incHyst < TEMP_HYST_MIN) {
    settings.incHyst = TEMP_HYST_MIN;
  } else if (settings.incHyst > TEMP_HYST_MAX) {
    settings.incHyst = TEMP_HYST_MAX;
  }
  if (settings.lightBrightness > 100) {
    settings.lightBrightness = 100;
  }
  settings.lightBrightness = (uint8_t)(((settings.lightBrightness + 5U) / 10U) * 10U);
  if (settings.rgbBrightness > 255) {
    settings.rgbBrightness = 255;
  }
}

static bool storage_acceptSettingsBlock(const SavedSettings &block) {
  if (!storage_blockCrcValid(block)) {
    return false;
  }
  settings = block;
  storage_sanitizeSettings();
  if (!storage_blockFieldsValid(settings)) {
    return false;
  }
  return true;
}

static bool storage_tryAcceptV22Bank(uint8_t bank) {
  SavedSettingsV22 block = {};
  const size_t bytesRead = prefs.getBytes(storage_bankKey(bank), &block, sizeof(block));
  if (bytesRead != sizeof(block)) {
    return false;
  }
  if (!storage_blockCrcValidV22(block) || !storage_blockFieldsValidV22(block)) {
    return false;
  }
  return storage_migrateV22ToCurrent(block);
}

static bool storage_tryAcceptV21Bank(uint8_t bank) {
  SavedSettingsV21 block = {};
  const size_t bytesRead = prefs.getBytes(storage_bankKey(bank), &block, sizeof(block));
  if (bytesRead != sizeof(block)) {
    return false;
  }
  if (!storage_blockCrcValidV21(block) || !storage_blockFieldsValidV21(block)) {
    return false;
  }
  return storage_migrateV21ToCurrent(block);
}

static void storage_prepareForSave() {
  storage_sanitizeSettings();
  settings.signature = SETTINGS_SIG;
  settings.version = SETTINGS_VER;
  storage_refreshSettingsCrc();
}

static bool storage_metaValid(const StorageMeta &meta) {
  if (meta.magic != STORAGE_META_MAGIC) {
    return false;
  }
  if (meta.activeBank > 1) {
    return false;
  }
  return storage_metaCrc(meta) == meta.crc16;
}

static bool storage_readMeta(StorageMeta &meta) {
  return prefs.getBytes(KEY_META, &meta, sizeof(meta)) == sizeof(meta);
}

static bool storage_writeMeta(const StorageMeta &meta) {
  return prefs.putBytes(KEY_META, &meta, sizeof(meta)) == sizeof(meta);
}

static bool storage_readBank(uint8_t bank, SavedSettings &out) {
  return prefs.getBytes(storage_bankKey(bank), &out, sizeof(out)) == sizeof(out);
}

static bool storage_writeBank(uint8_t bank, const SavedSettings &block) {
  return prefs.putBytes(storage_bankKey(bank), &block, sizeof(block)) == sizeof(block);
}

static bool storage_tryLoadBank(uint8_t bank) {
  SavedSettings block = {};
  if (storage_readBank(bank, block) && storage_acceptSettingsBlock(block)) {
    return true;
  }
  if (storage_tryAcceptV24Bank(bank) || storage_tryAcceptV23Bank(bank) || storage_tryAcceptV22Bank(bank) ||
      storage_tryAcceptV21Bank(bank)) {
    storage_writeBank(bank, settings);
    return true;
  }
  return false;
}

// Open NVS namespace at boot and after I2C recovery.
void storage_begin() {
  if (storageReady) {
    prefs.end();
    storageReady = false;
  }
  storageReady = prefs.begin(NVS_NS, false);
}

bool storage_isReady() {
  return storageReady;
}

uint16_t storage_loadDstAppliedKey() {
  if (!storageReady) {
    return 0;
  }
  return (uint16_t)prefs.getUInt(KEY_DST_APPLIED, 0);
}

void storage_saveDstAppliedKey(uint16_t key) {
  if (!storageReady) {
    return;
  }
  prefs.putUInt(KEY_DST_APPLIED, key);
}

uint16_t storage_calculateCRC16(const uint8_t *data, uint16_t length) {
  uint16_t crc = 0xFFFF;
  for (uint16_t i = 0; i < length; i++) {
    crc ^= data[i];
    for (uint8_t j = 0; j < 8; j++) {
      if (crc & 0x0001) {
        crc = (crc >> 1) ^ 0xA001;
      } else {
        crc = crc >> 1;
      }
    }
    if (i % 16 == 0) {
      yield();
    }
  }
  return crc;
}

// Factory default settings (when NVS is empty or corrupt).
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

// Write settings to inactive NVS bank with CRC verification.
bool storage_save() {
  lastSaveFailStep = 0;
  if (!storageReady) {
    lastSaveFailStep = 1;
    return false;
  }

  storage_prepareForSave();

  StorageMeta meta = {};
  uint8_t targetBank = 0;
  if (storage_readMeta(meta) && storage_metaValid(meta)) {
    targetBank = (meta.activeBank == 0) ? 1 : 0;
    meta.seq++;
  } else {
    targetBank = 0;
    meta.seq = 0;
  }

  if (!storage_writeBank(targetBank, settings)) {
    lastSaveFailStep = 2;
    return false;
  }

  SavedSettings verify = {};
  if (!storage_readBank(targetBank, verify)) {
    lastSaveFailStep = 3;
    return false;
  }
  if (!storage_acceptSettingsBlock(verify)) {
    lastSaveFailStep = 4;
    return false;
  }

  meta.magic = STORAGE_META_MAGIC;
  meta.activeBank = targetBank;
  meta.crc16 = storage_metaCrc(meta);
  if (!storage_writeMeta(meta)) {
    lastSaveFailStep = 5;
    return false;
  }

  StorageMeta verifyMeta = {};
  if (!storage_readMeta(verifyMeta) || !storage_metaValid(verifyMeta)) {
    lastSaveFailStep = 6;
    return false;
  }
  if (verifyMeta.activeBank != targetBank) {
    lastSaveFailStep = 7;
    return false;
  }

  return true;
}

void storage_requestSave() {
  if (!storageReady) {
    return;
  }
  if (storage_save()) {
    savePending = false;
    return;
  }
  savePending = true;
  saveRequestedAt = millis();
}

void storage_pollSave() {
  if (!savePending || !storageReady) {
    return;
  }
  if (millis() - saveRequestedAt < STORAGE_SAVE_DEBOUNCE_MS) {
    return;
  }
  if (storage_save()) {
    savePending = false;
  }
}

void storage_logSaveFailure() {
  Serial.print(F("Settings: NVS save failed step "));
  Serial.println(lastSaveFailStep);
}

void storage_logLoadFailure() {
  if (!storageReady) {
    Serial.println(F("Settings: NVS not available"));
    return;
  }

  StorageMeta meta = {};
  const bool metaRead = storage_readMeta(meta);
  Serial.print(F("Settings: meta "));
  Serial.print(metaRead ? F("read ok") : F("missing"));
  if (metaRead) {
    Serial.print(F(" magic=0x"));
    Serial.print(meta.magic, HEX);
    Serial.print(F(" bank="));
    Serial.print(meta.activeBank);
    Serial.print(F(" valid="));
    Serial.println(storage_metaValid(meta) ? F("yes") : F("no"));
  } else {
    Serial.println();
  }

  for (uint8_t bank = 0; bank <= 1; bank++) {
    SavedSettings block = {};
    const bool readOk = storage_readBank(bank, block);
    Serial.print(F("  bank "));
    Serial.print(bank);
    Serial.print(F(": "));
    Serial.print(readOk ? F("present") : F("empty"));
    if (!readOk) {
      Serial.println();
      continue;
    }
    Serial.print(F(" sig=0x"));
    Serial.print(block.signature, HEX);
    Serial.print(F(" ver="));
    Serial.print(block.version);
    Serial.print(F(" crc="));
    Serial.print(storage_blockCrcValid(block) ? F("ok") : F("bad"));
    Serial.print(F(" fields="));
    Serial.println(storage_blockFieldsValid(block) ? F("ok") : F("bad"));
  }
}

// Load settings from active bank; migrate v21/v22 when format is legacy.
bool storage_load() {
  if (!storageReady) {
    return false;
  }

  StorageMeta meta = {};
  const bool metaOk = storage_readMeta(meta) && storage_metaValid(meta);

  if (metaOk && storage_tryLoadBank(meta.activeBank)) {
    return true;
  }

  for (uint8_t bank = 0; bank <= 1; bank++) {
    if (metaOk && bank == meta.activeBank) {
      continue;
    }
    if (storage_tryLoadBank(bank)) {
      meta.magic = STORAGE_META_MAGIC;
      meta.activeBank = bank;
      meta.seq = metaOk ? (uint8_t)(meta.seq + 1) : 0;
      meta.crc16 = storage_metaCrc(meta);
      storage_writeMeta(meta);
      return true;
    }
  }

  return false;
}
