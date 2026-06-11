/*
 * rtc_clock.h — DS3231 real-time clock
 *
 * Holds current date and time for lighting schedule, display, and sensor log.
 * Supports clock editing from the menu and daylight saving time (DST) transitions.
 */

#ifndef RTC_CLOCK_H
#define RTC_CLOCK_H

#include <RTClib.h>

extern RTC_DS3231 rtc;
extern bool rtcError;
extern DateTime rtcEditDraft;
extern DateTime rtcNowCached;
extern bool rtcNowValid;

bool rtc_initHardware();
void rtc_service(unsigned long nowMillis);
void rtc_updateCache(bool forceRefresh = false);
void rtc_pollDst();
const DateTime &rtc_getNow();
bool rtc_hasValidNow();

void rtc_getFieldLimits(int8_t field, int16_t &minv, int16_t &maxv);
void rtc_normalizeDraft();

void rtc_beginClockEdit();
bool rtc_commitClockEdit();
void rtc_setEditSecond(uint8_t second);
void rtc_cancelClockEdit();
int16_t rtc_getEditField(int8_t field);
void rtc_setEditField(int8_t field, int16_t value);
bool rtc_applyEditField(int8_t field, int16_t value);

#endif // RTC_CLOCK_H
