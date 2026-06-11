/*
 * sensor_log.h — sensor reading log on flash (LittleFS)
 *
 * Periodically records temperature and humidity for web interface charts.
 * When SENSOR_LOG_ENABLE=0 or enabled=0 in NVS, the module does not use flash
 * and does not write data.
 */

#ifndef SENSOR_LOG_H
#define SENSOR_LOG_H

#include "config.h"
#include <Arduino.h>

enum SensorLogSeries : uint8_t {
  SENSOR_LOG_INC = 0,
  SENSOR_LOG_GRH_T = 1,
  SENSOR_LOG_GRH_H = 2,
  SENSOR_LOG_COUNT = 3
};

#if SENSOR_LOG_ENABLE && defined(ARDUINO_ARCH_ESP32) && !defined(SIM_BUILD)

void sensor_log_begin();
void sensor_log_poll(unsigned long nowMillis);
bool sensor_log_isCompiledIn();
bool sensor_log_isReady();
bool sensor_log_isEnabled();
void sensor_log_setEnabled(bool enabled);
uint8_t sensor_log_getIntervalMin();
bool sensor_log_setIntervalMin(uint8_t minutes);
bool sensor_log_clear();
uint32_t sensor_log_recordCount(SensorLogSeries series);
const char *sensor_log_getConfigJson();
const char *sensor_log_getHistoryJson(SensorLogSeries series, uint32_t hours, uint16_t maxPoints);
const char *sensor_log_seriesName(SensorLogSeries series);
const char *sensor_log_seriesLabel(SensorLogSeries series);
bool sensor_log_parseSeries(const char *name, SensorLogSeries &out);

#else

inline void sensor_log_begin() {}
inline void sensor_log_poll(unsigned long nowMillis) { (void)nowMillis; }
inline bool sensor_log_isCompiledIn() { return false; }
inline bool sensor_log_isReady() { return false; }
inline bool sensor_log_isEnabled() { return false; }
inline void sensor_log_setEnabled(bool enabled) { (void)enabled; }
inline uint8_t sensor_log_getIntervalMin() { return 5; }
inline bool sensor_log_setIntervalMin(uint8_t minutes) { (void)minutes; return false; }
inline bool sensor_log_clear() { return false; }
inline uint32_t sensor_log_recordCount(SensorLogSeries series) { (void)series; return 0; }
#if !defined(SIM_BUILD)
inline const char *sensor_log_getConfigJson() { return "{\"compiled_in\":false}"; }
inline const char *sensor_log_getHistoryJson(SensorLogSeries series, uint32_t hours, uint16_t maxPoints) {
  (void)series;
  (void)hours;
  (void)maxPoints;
  return "{\"ok\":false,\"error\":\"disabled\"}";
}
#endif
inline const char *sensor_log_seriesName(SensorLogSeries series) {
  static const char *kNames[] = {"inc", "grh_t", "grh_h"};
  return series < SENSOR_LOG_COUNT ? kNames[series] : "";
}
inline const char *sensor_log_seriesLabel(SensorLogSeries series) {
  static const char *kLabels[] = {"INC temperature", "GRH temperature", "GRH humidity"};
  return series < SENSOR_LOG_COUNT ? kLabels[series] : "";
}
inline bool sensor_log_parseSeries(const char *name, SensorLogSeries &out) {
  (void)name;
  (void)out;
  return false;
}

#endif

#endif // SENSOR_LOG_H
