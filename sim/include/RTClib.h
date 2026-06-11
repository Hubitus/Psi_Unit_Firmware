/*
 * RTClib.h — minimal DateTime for simulator (no Wire / DS3231)
 */

#ifndef RTCLIB_H
#define RTCLIB_H

#include <stdint.h>

class DateTime {
public:
  DateTime(uint16_t year = 2000, uint8_t month = 1, uint8_t day = 1, uint8_t hour = 0, uint8_t minute = 0,
           uint8_t second = 0);
  explicit DateTime(uint32_t unixTime);

  uint16_t year() const {
    return _y;
  }
  uint8_t month() const {
    return _m;
  }
  uint8_t day() const {
    return _d;
  }
  uint8_t hour() const {
    return _hh;
  }
  uint8_t minute() const {
    return _mm;
  }
  uint8_t second() const {
    return _ss;
  }
  uint32_t unixtime() const {
    return _unix;
  }
  uint8_t dayOfTheWeek() const;

private:
  uint16_t _y;
  uint8_t _m;
  uint8_t _d;
  uint8_t _hh;
  uint8_t _mm;
  uint8_t _ss;
  uint32_t _unix;
  void recalcUnix();
};

struct RTC_DS3231 {
  bool begin(void *) {
    return true;
  }
  bool begin() {
    return true;
  }
  void adjust(const DateTime &) {}
};

#endif // RTCLIB_H
