#include "RTClib.h"

static bool isLeap(uint16_t y) {
  return (y % 4 == 0 && y % 100 != 0) || (y % 400 == 0);
}

static uint8_t daysInMonth(uint16_t y, uint8_t m) {
  static const uint8_t d[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  if (m < 1 || m > 12) {
    return 31;
  }
  uint8_t n = d[m - 1];
  if (m == 2 && isLeap(y)) {
    n = 29;
  }
  return n;
}

static uint32_t toUnix(uint16_t y, uint8_t m, uint8_t d, uint8_t h, uint8_t mi, uint8_t s) {
  uint32_t days = 0;
  for (uint16_t year = 1970; year < y; year++) {
    days += isLeap(year) ? 366U : 365U;
  }
  for (uint8_t month = 1; month < m; month++) {
    days += daysInMonth(y, month);
  }
  days += (uint32_t)(d - 1);
  return days * 86400UL + (uint32_t)h * 3600UL + (uint32_t)mi * 60UL + s;
}

static void fromUnix(uint32_t unix, uint16_t &y, uint8_t &m, uint8_t &d, uint8_t &h, uint8_t &mi, uint8_t &s) {
  uint32_t rem = unix;
  s = (uint8_t)(rem % 60);
  rem /= 60;
  mi = (uint8_t)(rem % 60);
  rem /= 60;
  const uint32_t days = rem / 24;
  h = (uint8_t)(rem % 24);

  y = 1970;
  uint32_t left = days;
  while (true) {
    const uint32_t yearDays = isLeap(y) ? 366U : 365U;
    if (left < yearDays) {
      break;
    }
    left -= yearDays;
    y++;
  }
  m = 1;
  while (m <= 12) {
    const uint32_t md = daysInMonth(y, m);
    if (left < md) {
      break;
    }
    left -= md;
    m++;
  }
  d = (uint8_t)(left + 1);
}

DateTime::DateTime(uint16_t year, uint8_t month, uint8_t day, uint8_t hour, uint8_t minute, uint8_t second)
    : _y(year), _m(month), _d(day), _hh(hour), _mm(minute), _ss(second) {
  recalcUnix();
}

DateTime::DateTime(uint32_t unixTime) : _unix(unixTime) {
  fromUnix(_unix, _y, _m, _d, _hh, _mm, _ss);
}

void DateTime::recalcUnix() {
  _unix = toUnix(_y, _m, _d, _hh, _mm, _ss);
}

uint8_t DateTime::dayOfTheWeek() const {
  const uint32_t days = _unix / 86400UL;
  return (uint8_t)((days + 4) % 7);
}
