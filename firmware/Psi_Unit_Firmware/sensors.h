/*
 * sensors.h — temperature and humidity sensors
 *
 * Current readings (DS18B20 — INC, FS400-SHT30 — GRH), error flags,
 * and poll functions. Used by control logic, display, and Wi‑Fi.
 */

#ifndef SENSORS_H
#define SENSORS_H

#include "config.h"
#include "types.h"

class DallasTemperature;

extern int16_t tDS;
extern int16_t tSHT;
extern int16_t hSHT;
extern SensorState ds18State;
extern SensorState shtTempState;
extern SensorState shtHumState;
extern bool dsError;
extern bool tempError;
extern bool humError;

extern DallasTemperature* ds18;
extern bool ds18Present;

inline bool sensor_hasValue(int16_t value) {
  return value != SENSOR_INVALID;
}

bool sensor_validateTemp(int16_t temp);
bool sensor_validateHum(int16_t hum);
int16_t sensor_smoothEma(int16_t current, int16_t newSample);
bool sensor_displayValueChanged(int16_t prev, int16_t next, int16_t minDelta);
void sensor_updateState(SensorState &state, bool success);
void sensor_invalidateData(int16_t &value, SensorState &state);
void sensor_updateErrorFlags();

bool sensors_initDs18();
bool sensors_initSht30();
bool sensors_isShtConnected();
void sensors_reinitSht30();
bool sensors_pollDs18Reconnect(unsigned long nowMillis);
bool sensors_ds18ConversionReady();
void sensors_poll(unsigned long nowMillis);

#endif // SENSORS_H
