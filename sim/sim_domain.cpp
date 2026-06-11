/*
 * sim_domain.cpp — sensors, logic, rgb, i2c (stubs for UI simulator)
 */

#include "sensors.h"
#include "logic.h"
#include "rgb_status.h"
#include "i2c_bus.h"
#include "config.h"
#include "storage.h"
#include <Arduino.h>
#include <stdlib.h>

int16_t tDS = 250;
int16_t tSHT = 242;
int16_t hSHT = 58;
SensorState ds18State = {true, 0, 0, true};
SensorState shtTempState = {true, 0, 0, true};
SensorState shtHumState = {true, 0, 0, true};
bool dsError = false;
bool tempError = false;
bool humError = false;
DallasTemperature *ds18 = nullptr;
bool ds18Present = true;

bool sensor_validateTemp(int16_t temp) {
  return temp >= TEMP_MIN && temp <= TEMP_MAX;
}

bool sensor_validateHum(int16_t hum) {
  return hum >= HUM_MIN && hum <= HUM_MAX;
}

int16_t sensor_smoothEma(int16_t current, int16_t newSample) {
  if (current == SENSOR_INVALID) {
    return newSample;
  }
  return (int16_t)(((int32_t)current * SMOOTH_BETA + (int32_t)newSample * SMOOTH_ALPHA) / 100);
}

bool sensor_displayValueChanged(int16_t prev, int16_t next, int16_t minDelta) {
  if (prev == SENSOR_INVALID || next == SENSOR_INVALID) {
    return prev != next;
  }
  return (int16_t)abs(next - prev) >= minDelta;
}

void sensor_updateState(SensorState &state, bool success) {
  if (success) {
    state.errorCount = 0;
    state.dataValid = true;
    state.lastValidRead = millis();
  } else if (state.errorCount < 255) {
    state.errorCount++;
  }
}

void sensor_invalidateData(int16_t &value, SensorState &state) {
  value = SENSOR_INVALID;
  state.dataValid = false;
}

void sensor_updateErrorFlags() {}

bool sensors_initDs18() {
  return true;
}

bool sensors_initSht30() {
  return true;
}

bool sensors_isShtConnected() {
  return true;
}

void sensors_reinitSht30() {}

bool sensors_pollDs18Reconnect(unsigned long) {
  return false;
}

bool sensors_ds18ConversionReady() {
  return true;
}

void sensors_poll(unsigned long) {}

void rgb_status_begin() {}
void rgb_status_poll(unsigned long) {}
void rgb_status_applySettings() {}
void rgb_status_setBrightnessPreview(uint8_t, bool) {}
RgbStatusMode rgb_status_getMode() {
  return RGB_MODE_OK;
}

void i2c_init(bool) {}
void i2c_softReinit() {}
bool i2c_probe(uint8_t, uint8_t *) {
  return true;
}
void i2c_noteTransmission(uint8_t) {}
void i2c_poll(unsigned long, bool) {}
bool i2c_isHealthy() {
  return true;
}
uint8_t i2c_failureCount() {
  return 0;
}
bool i2c_probeDevices(bool) {
  return true;
}
bool i2c_isOledPresent() {
  return true;
}
uint8_t i2c_getOledAddress() {
  return OLED_I2C_ADDR;
}
bool i2c_isRtcPresent() {
  return true;
}
void i2c_recover() {}
void i2c_setPeripheralReinit(void (*)(void)) {}
void i2c_setRtcStalePredicate(I2cRtcStalePredicate) {}
