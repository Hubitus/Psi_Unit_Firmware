/*
 * sim_rtc.cpp — software clock for desktop simulator
 */

#include "rtc_clock.h"
#include "config.h"
#include <Arduino.h>

RTC_DS3231 rtc;
bool rtcError = false;
DateTime rtcEditDraft(2026, 5, 30, 12, 0, 0);
DateTime rtcNowCached(2026, 5, 30, 12, 0, 0);
bool rtcNowValid = true;

static const int8_t RTC_FIELD_COUNT = 5;
static const int16_t RTC_LIMITS[5][2] = {{2000, 2099}, {1, 12}, {1, 31}, {0, 23}, {0, 59}};

static unsigned long rtcHwSyncMs = 0;
static DateTime rtcHwBase(2026, 5, 30, 12, 0, 0);

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

static void rtc_storeNow(const DateTime &t) {
  rtcHwBase = t;
  rtcHwSyncMs = millis();
  rtcNowCached = t;
  rtcNowValid = true;
}

static void rtc_tickFromMillis() {
  if (!rtcNowValid) {
    return;
  }
  if (rtcHwSyncMs == 0) {
    rtcHwSyncMs = millis();
    if (rtcHwSyncMs == 0) {
      rtcHwSyncMs = 1;
    }
    return;
  }
  const unsigned long elapsedSec = (millis() - rtcHwSyncMs) / 1000UL;
  if (elapsedSec == 0) {
    return;
  }
  rtcNowCached = DateTime(static_cast<uint32_t>(rtcHwBase.unixtime() + elapsedSec));
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
  return DateTime((uint16_t)y, (uint8_t)m, (uint8_t)d, (uint8_t)h, (uint8_t)min, (uint8_t)s);
}

bool rtc_initHardware() {
  rtcError = false;
  rtc_storeNow(rtcNowCached);
  return true;
}

void rtc_service(unsigned long) {}

void rtc_updateCache(bool forceRefresh) {
  (void)forceRefresh;
  rtc_tickFromMillis();
}

void rtc_pollDst() {}

const DateTime &rtc_getNow() {
  rtc_tickFromMillis();
  return rtcNowCached;
}

bool rtc_hasValidNow() {
  return rtcNowValid && !rtcError;
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

void rtc_normalizeDraft() {
  int16_t y = rtcEditDraft.year();
  int16_t m = constrain(rtcEditDraft.month(), RTC_LIMITS[1][0], RTC_LIMITS[1][1]);
  int16_t d = constrain(rtcEditDraft.day(), 1, rtc_daysInMonth((uint16_t)y, (uint8_t)m));
  int16_t h = constrain(rtcEditDraft.hour(), RTC_LIMITS[3][0], RTC_LIMITS[3][1]);
  int16_t min = constrain(rtcEditDraft.minute(), RTC_LIMITS[4][0], RTC_LIMITS[4][1]);
  int16_t s = rtcEditDraft.second();
  rtcEditDraft = DateTime((uint16_t)y, (uint8_t)m, (uint8_t)d, (uint8_t)h, (uint8_t)min, (uint8_t)s);
}

void rtc_beginClockEdit() {
  rtc_updateCache(true);
  rtcEditDraft = rtc_hasValidNow() ? rtcNowCached : rtcEditDraft;
  rtc_normalizeDraft();
}

bool rtc_commitClockEdit() {
  rtc_normalizeDraft();
  rtc_storeNow(rtcEditDraft);
  rtcError = false;
  return rtc_hasValidNow();
}

void rtc_setEditSecond(uint8_t second) {
  second = (uint8_t)constrain((int)second, 0, 59);
  rtcEditDraft = DateTime(rtcEditDraft.year(), rtcEditDraft.month(), rtcEditDraft.day(),
                          rtcEditDraft.hour(), rtcEditDraft.minute(), second);
  rtc_normalizeDraft();
}

void rtc_cancelClockEdit() {
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
