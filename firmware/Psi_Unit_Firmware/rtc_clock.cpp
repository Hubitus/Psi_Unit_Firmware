/*
 * rtc_clock.cpp — DS3231 real-time clock over I2C (RTClib)
 *
 * Reads time from the chip and caches it for the display (without redundant I2C
 * access). Allows setting date/time from the menu. Direct register reads are
 * required for correct operation when the chip is in 12-hour mode.
 */

#include "rtc_clock.h"
#include "config.h"
#include "storage.h"
#include "i2c_bus.h"
#include <Arduino.h>
#include <Wire.h>

#if !defined(ARDUINO_ARCH_ESP32)
#error "rtc_clock.cpp: Arduino Nano ESP32 only."
#endif

static const uint8_t RTC_INIT_ATTEMPTS = 6;
static const uint8_t RTC_INIT_RETRY_MS = 50;
static const unsigned long RTC_CACHE_INTERVAL_MS = 1000UL;
static const unsigned long RTC_SERVICE_INTERVAL_MS = 5000UL;

static unsigned long rtcLastCacheMs = 0;
static unsigned long rtcLastServiceMs = 0;
static unsigned long rtcHwSyncMs = 0;
static DateTime rtcHwBase(2000, 1, 1, 0, 0, 0);
static bool rtc_chipLostPower = false;
static bool rtc_libReady = false;
static bool rtc_chipMaintained = false;

static void rtc_bootLog(const __FlashStringHelper *msg) {
  Serial.print(F("RTC: "));
  Serial.println(msg);
}

RTC_DS3231 rtc;
bool rtcError = false;
DateTime rtcEditDraft(2000, 1, 1, 0, 0, 0);
DateTime rtcNowCached(2000, 1, 1, 0, 0, 0);
bool rtcNowValid = false;

static const int8_t RTC_FIELD_COUNT = 5;

static const int16_t RTC_LIMITS[RTC_FIELD_COUNT][2] = {
  {2000, 2099},
  {1, 12},
  {1, 31},
  {0, 23},
  {0, 59},
};

static bool rtc_isLeapYear(uint16_t year) {
  return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
}

static uint8_t rtc_daysInMonth(uint16_t year, uint8_t month) {
  static const uint8_t days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  if (month < 1 || month > 12) {
    return 31;
  }
  uint8_t d = days[month - 1];
  if (month == 2 && rtc_isLeapYear(year)) {
    d = 29;
  }
  return d;
}

static bool rtc_yearInRange(uint16_t y) {
  return y >= 2000 && y <= 2099;
}

static uint8_t rtc_bcd2bin(uint8_t val) {
  return val - 6 * (val >> 4);
}

static uint8_t rtc_bin2bcd(uint8_t val) {
  return (uint8_t)(((val / 10U) << 4) | (val % 10U));
}

// Read time directly from DS3231 (correct when the chip uses 12-hour format).
static DateTime rtc_decodeDs3231Buffer(const uint8_t buffer[7]) {
  const uint8_t sec = rtc_bcd2bin(buffer[0] & 0x7F);
  const uint8_t min = rtc_bcd2bin(buffer[1] & 0x7F);
  const uint8_t hrReg = buffer[2];
  uint8_t hour;
  if (hrReg & 0x40) {
    const bool pm = (hrReg & 0x20) != 0;
    uint8_t h12 = rtc_bcd2bin(hrReg & 0x1F);
    if (h12 < 1 || h12 > 12) {
      h12 = 12;
    }
    if (h12 == 12) {
      hour = pm ? 12 : 0;
    } else {
      hour = pm ? (uint8_t)(h12 + 12) : h12;
    }
  } else {
    hour = rtc_bcd2bin(hrReg & 0x3F);
  }
  const uint8_t day = rtc_bcd2bin(buffer[4]);
  const uint8_t month = rtc_bcd2bin(buffer[5] & 0x7F);
  const uint16_t year = (uint16_t)(rtc_bcd2bin(buffer[6]) + 2000U);
  return DateTime(year, month, day, hour, min, sec);
}

static bool rtc_readDs3231Registers(uint8_t buffer[7]) {
  Wire.beginTransmission(RTC_I2C_ADDR);
  Wire.write((uint8_t)0);
  if (Wire.endTransmission() != 0) {
    return false;
  }
  if (Wire.requestFrom((uint8_t)RTC_I2C_ADDR, (uint8_t)7, (uint8_t)1) != 7 ||
      Wire.available() < 7) {
    return false;
  }
  for (uint8_t i = 0; i < 7; i++) {
    buffer[i] = Wire.read();
  }
  return true;
}

static bool rtc_ensureDriver() {
  if (rtc_libReady) {
    return true;
  }
  i2c_softReinit();
  rtc_libReady = rtc.begin(&Wire);
  return rtc_libReady;
}

static void rtc_force24HourModeIfNeeded() {
  uint8_t buffer[7];
  if (!rtc_readDs3231Registers(buffer)) {
    return;
  }
  if ((buffer[2] & 0x40) == 0) {
    return;
  }
  if (!rtc_ensureDriver()) {
    return;
  }
  rtc.adjust(rtc_decodeDs3231Buffer(buffer));
  rtc_bootLog(F("12h -> 24h mode"));
}

static bool rtc_timeFieldsValid(const DateTime &t) {
  if (!rtc_yearInRange(t.year())) {
    return false;
  }
  const uint8_t m = t.month();
  const uint8_t d = t.day();
  if (m < 1 || m > 12 || d < 1 || d > rtc_daysInMonth(t.year(), m)) {
    return false;
  }
  return t.hour() <= 23 && t.minute() <= 59 && t.second() <= 59;
}

static bool rtc_writeChipTime(const DateTime &t) {
  if (!rtc_timeFieldsValid(t)) {
    return false;
  }
  Wire.beginTransmission(RTC_I2C_ADDR);
  Wire.write((uint8_t)0);
  Wire.write(rtc_bin2bcd(t.second()));
  Wire.write(rtc_bin2bcd(t.minute()));
  Wire.write(rtc_bin2bcd(t.hour()));
  Wire.write((uint8_t)1);
  Wire.write(rtc_bin2bcd(t.day()));
  Wire.write(rtc_bin2bcd(t.month()));
  Wire.write(rtc_bin2bcd((uint8_t)(t.year() - 2000U)));
  return Wire.endTransmission() == 0;
}

static bool rtc_stepPlausible(const DateTime &t) {
  if (!rtcNowValid) {
    return true;
  }
  const int32_t delta = (int32_t)t.unixtime() - (int32_t)rtcNowCached.unixtime();
  // Time jump within a reasonable range (missed poll while I2C was busy).
  if (delta >= -120 && delta <= 120) {
    return true;
  }
  if (delta == 3600 || delta == -3600) {
    return true;
  }
  if (rtcNowCached.year() < 2020 && t.year() >= 2020) {
    return true;
  }
  return false;
}

static bool rtc_readChipTime(DateTime &out, bool strictStep) {
  uint8_t buf[7];
  if (!rtc_readDs3231Registers(buf)) {
    return false;
  }
  out = rtc_decodeDs3231Buffer(buf);
  if (!rtc_timeFieldsValid(out)) {
    return false;
  }
  if (strictStep && !rtc_stepPlausible(out)) {
    return false;
  }
  return true;
}

static int16_t rtc_fieldFromDateTime(const DateTime &dt, int8_t field) {
  switch (field) {
    case 0: return dt.year();
    case 1: return dt.month();
    case 2: return dt.day();
    case 3: return dt.hour();
    case 4: return dt.minute();
    default: return 0;
  }
}

static DateTime rtc_dateTimeWithField(const DateTime &base, int8_t field, int16_t value) {
  int16_t y = base.year();
  int16_t m = base.month();
  int16_t d = base.day();
  int16_t h = base.hour();
  int16_t min = base.minute();
  int16_t s = base.second();

  switch (field) {
    case 0: y = value; break;
    case 1: m = value; break;
    case 2: d = value; break;
    case 3: h = value; break;
    case 4: min = value; break;
    default: break;
  }

  y = constrain(y, RTC_LIMITS[0][0], RTC_LIMITS[0][1]);
  m = constrain(m, RTC_LIMITS[1][0], RTC_LIMITS[1][1]);
  d = constrain(d, 1, rtc_daysInMonth((uint16_t)y, (uint8_t)m));
  h = constrain(h, RTC_LIMITS[3][0], RTC_LIMITS[3][1]);
  min = constrain(min, RTC_LIMITS[4][0], RTC_LIMITS[4][1]);

  return DateTime(y, m, d, h, min, s);
}

void rtc_normalizeDraft() {
  int16_t y = rtcEditDraft.year();
  int16_t m = constrain(rtcEditDraft.month(), RTC_LIMITS[1][0], RTC_LIMITS[1][1]);
  int16_t d = constrain(rtcEditDraft.day(), 1, rtc_daysInMonth((uint16_t)y, (uint8_t)m));
  int16_t h = constrain(rtcEditDraft.hour(), RTC_LIMITS[3][0], RTC_LIMITS[3][1]);
  int16_t min = constrain(rtcEditDraft.minute(), RTC_LIMITS[4][0], RTC_LIMITS[4][1]);
  int16_t s = rtcEditDraft.second();
  rtcEditDraft = DateTime(y, m, d, h, min, s);
}

static void rtc_clearClockHaltIfNeeded() {
  uint8_t buf[7];
  if (!rtc_readDs3231Registers(buf)) {
    return;
  }
  if ((buf[0] & 0x80) == 0) {
    return;
  }
  Wire.beginTransmission(RTC_I2C_ADDR);
  Wire.write((uint8_t)0);
  Wire.write((uint8_t)(buf[0] & 0x7F));
  if (Wire.endTransmission() == 0) {
    rtc_bootLog(F("clock halt cleared"));
  }
}

static void rtc_storeNow(const DateTime &t) {
  rtcHwBase = t;
  rtcHwSyncMs = millis();
  rtcNowCached = t;
  rtcNowValid = true;
  rtcLastCacheMs = rtcHwSyncMs;
}

static void rtc_tickFromMillis() {
  if (!rtcNowValid || rtcHwSyncMs == 0) {
    return;
  }
  const unsigned long elapsedSec = (millis() - rtcHwSyncMs) / 1000UL;
  if (elapsedSec == 0) {
    return;
  }
  rtcNowCached = DateTime(rtcHwBase.unixtime() + elapsedSec);
}

#if RTC_DST_EU
static uint16_t rtc_lastDstAppliedKey = 0;
static bool rtc_dstKeyLoaded = false;

static void rtc_ensureDstKeyLoaded() {
  if (!rtc_dstKeyLoaded) {
    rtc_lastDstAppliedKey = storage_loadDstAppliedKey();
    rtc_dstKeyLoaded = true;
  }
}

static uint8_t rtc_lastSundayOfMonth(uint16_t year, uint8_t month) {
  uint8_t lastDay = rtc_daysInMonth(year, month);
  DateTime probe(year, month, lastDay, 12, 0, 0);
  return lastDay - probe.dayOfTheWeek();
}

static uint16_t rtc_dstTransitionKey(uint16_t year, uint8_t transition) {
  return (uint16_t)((year << 4) | (transition & 0x0F));
}

static void rtc_applyDstStep(const DateTime &t, int32_t deltaSec, uint16_t key) {
  if (!rtc_ensureDriver()) {
    return;
  }
  rtc.adjust(DateTime((uint32_t)((int32_t)t.unixtime() + deltaSec)));
  rtc_lastDstAppliedKey = key;
  storage_saveDstAppliedKey(key);
  DateTime after;
  if (rtc_readChipTime(after, false)) {
    rtc_storeNow(after);
  }
}

void rtc_pollDst() {
  if (rtcError || !rtcNowValid) {
    return;
  }
  rtc_ensureDstKeyLoaded();
  const DateTime &t = rtcNowCached;
  const uint16_t y = t.year();
  if (!rtc_yearInRange(y)) {
    return;
  }

  const uint8_t lastSunMarch = rtc_lastSundayOfMonth(y, 3);
  const uint8_t lastSunOctober = rtc_lastSundayOfMonth(y, 10);

  if (t.month() == 3 && t.day() == lastSunMarch && t.hour() == 3 && t.minute() < 3) {
    const uint16_t key = rtc_dstTransitionKey(y, 1);
    if (key != rtc_lastDstAppliedKey) {
      rtc_applyDstStep(t, 3600, key);
    }
    return;
  }

  if (t.month() == 10 && t.day() == lastSunOctober && t.hour() == 4 && t.minute() < 3) {
    const uint16_t key = rtc_dstTransitionKey(y, 2);
    if (key != rtc_lastDstAppliedKey) {
      rtc_applyDstStep(t, -3600, key);
    }
  }
}
#else
void rtc_pollDst() {}
#endif

void rtc_updateCache(bool forceRefresh) {
  const unsigned long nowMs = millis();
  if (!forceRefresh && rtcNowValid && !rtcError &&
      (nowMs - rtcLastCacheMs) < RTC_CACHE_INTERVAL_MS) {
    return;
  }

  if (rtcError) {
    i2c_softReinit();
    if (!i2c_probe(RTC_I2C_ADDR)) {
      rtcLastCacheMs = nowMs;
      return;
    }
    rtcError = false;
  }

  DateTime t;
  if (rtc_readChipTime(t, false)) {
    rtc_storeNow(t);
#if RTC_DST_EU
    rtc_pollDst();
#endif
  } else {
    rtc_tickFromMillis();
  }
}

static bool rtc_tryReadAndStore() {
  DateTime t;
  if (!rtc_readChipTime(t, false)) {
    return false;
  }
  rtc_storeNow(t);
  rtcError = false;
  return true;
}

static bool rtc_applyChipMaintenance() {
  if (!rtc_ensureDriver()) {
    return false;
  }
  rtc_chipLostPower = rtc.lostPower();
  rtc_clearClockHaltIfNeeded();
  if (rtc_chipLostPower) {
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    rtc_bootLog(F("lost power, set compile time"));
  } else {
    rtc_force24HourModeIfNeeded();
  }
  return rtc_tryReadAndStore();
}

bool rtc_initHardware() {
  rtc_libReady = false;

  for (uint8_t attempt = 0; attempt < RTC_INIT_ATTEMPTS; attempt++) {
    if (attempt > 0) {
      delay(RTC_INIT_RETRY_MS);
      yield();
    }

    i2c_softReinit();
    delay(5);
    if (!i2c_probe(RTC_I2C_ADDR)) {
      continue;
    }

    rtcLastCacheMs = 0;
    if (rtc_tryReadAndStore()) {
      if (!rtc_chipMaintained) {
        rtc_chipMaintained = rtc_applyChipMaintenance();
      }
      return true;
    }
    rtc_bootLog(F("probe OK, time read failed"));
  }

  i2c_softReinit();
  if (rtc_tryReadAndStore()) {
    rtc_bootLog(F("init OK (read without probe)"));
    if (!rtc_chipMaintained) {
      rtc_chipMaintained = rtc_applyChipMaintenance();
    }
    return true;
  }

  rtcError = true;
  if (!rtcNowValid) {
    rtc_bootLog(F("init failed (no I2C response or bad time)"));
    uint8_t dbg[7];
    if (rtc_readDs3231Registers(dbg)) {
      Serial.print(F("RTC raw: "));
      for (uint8_t i = 0; i < 7; i++) {
        if (dbg[i] < 0x10) {
          Serial.print('0');
        }
        Serial.print(dbg[i], HEX);
        Serial.print(' ');
      }
      const DateTime dbgT = rtc_decodeDs3231Buffer(dbg);
      Serial.print(F("-> "));
      Serial.print(dbgT.year());
      Serial.print('-');
      Serial.print(dbgT.month());
      Serial.print('-');
      Serial.println(dbgT.day());
      if (rtc_timeFieldsValid(dbgT)) {
        rtc_storeNow(dbgT);
        rtcError = false;
        rtc_bootLog(F("init OK (diag decode)"));
        if (!rtc_chipMaintained) {
          rtc_chipMaintained = rtc_applyChipMaintenance();
        }
        return true;
      }
    }
  } else {
    rtcError = false;
    rtc_bootLog(F("init skipped, keeping cached time"));
    return true;
  }
  rtcNowValid = false;
  return false;
}

void rtc_service(unsigned long nowMillis) {
  if (!rtcError && rtcNowValid) {
    return;
  }
  if (rtcLastServiceMs != 0 && (nowMillis - rtcLastServiceMs) < RTC_SERVICE_INTERVAL_MS) {
    return;
  }
  rtcLastServiceMs = nowMillis;
  rtc_initHardware();
}

const DateTime &rtc_getNow() {
  rtc_tickFromMillis();
  return rtcNowCached;
}

bool rtc_hasValidNow() {
  return rtcNowValid;
}

void rtc_getFieldLimits(int8_t field, int16_t &minv, int16_t &maxv) {
  if (field < 0 || field >= RTC_FIELD_COUNT) {
    minv = 0;
    maxv = 0;
    return;
  }
  minv = RTC_LIMITS[field][0];
  maxv = RTC_LIMITS[field][1];
  if (field == 2) {
    maxv = rtc_daysInMonth((uint16_t)rtcEditDraft.year(), (uint8_t)rtcEditDraft.month());
  }
}

void rtc_beginClockEdit() {
  if (rtcError) {
    rtcEditDraft = DateTime(2000, 1, 1, 0, 0, 0);
    return;
  }
  rtc_updateCache(true);
  rtcEditDraft = rtc_hasValidNow() ? rtcNowCached : rtcEditDraft;
  rtc_normalizeDraft();
}

bool rtc_commitClockEdit() {
  rtc_normalizeDraft();
  if (!rtc_timeFieldsValid(rtcEditDraft)) {
    return false;
  }

  bool written = false;
  for (uint8_t attempt = 0; attempt < 3 && !written; attempt++) {
    if (attempt > 0) {
      i2c_softReinit();
      delay(2);
    }
    written = rtc_writeChipTime(rtcEditDraft);
    if (!written && rtc_ensureDriver()) {
      rtc.adjust(rtcEditDraft);
      written = true;
    }
  }
  if (!written) {
    return false;
  }

  rtc_storeNow(rtcEditDraft);
  rtcError = false;
  rtc_libReady = true;
  rtcLastCacheMs = 0;

  DateTime verify;
  if (rtc_readChipTime(verify, false)) {
    rtc_storeNow(verify);
  }
  return true;
}

void rtc_setEditSecond(uint8_t second) {
  second = (uint8_t)constrain((int)second, 0, 59);
  rtcEditDraft = DateTime(rtcEditDraft.year(), rtcEditDraft.month(), rtcEditDraft.day(),
                          rtcEditDraft.hour(), rtcEditDraft.minute(), second);
  rtc_normalizeDraft();
}

void rtc_cancelClockEdit() {
  if (rtcError) {
    rtcEditDraft = DateTime(2000, 1, 1, 0, 0, 0);
    return;
  }
  rtc_updateCache(true);
  rtcEditDraft = rtc_hasValidNow() ? rtcNowCached : rtcEditDraft;
  rtc_normalizeDraft();
}

int16_t rtc_getEditField(int8_t field) {
  return rtc_fieldFromDateTime(rtcEditDraft, field);
}

void rtc_setEditField(int8_t field, int16_t value) {
  rtcEditDraft = rtc_dateTimeWithField(rtcEditDraft, field, value);
  rtc_normalizeDraft();
}

bool rtc_applyEditField(int8_t field, int16_t value) {
  if (field < 0 || field >= RTC_FIELD_COUNT) {
    return false;
  }
  int16_t minv, maxv;
  rtc_getFieldLimits(field, minv, maxv);
  value = constrain(value, minv, maxv);
  rtc_setEditField(field, value);
  return true;
}
