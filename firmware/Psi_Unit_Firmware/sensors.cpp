/*
 * sensors.cpp — DS18B20 and FS400-SHT30 (SHT30) reading and processing
 *
 * Driver: Adafruit_SHT31 ("Adafruit SHT31 Library"; SHT30-compatible).
 * T and H from a single I2C measurement via readBoth().
 *
 * Polls sensors periodically, smooths values, tracks disconnects and reconnects.
 * On errors marks data invalid — AUTO heat and humidity then do not enable actuators.
 */

#include "sensors.h"
#include "app_state.h"
#include "app_platform.h"
#include "app_snapshot.h"
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Adafruit_SHT31.h>
#include <Arduino.h>

#if !defined(ARDUINO_ARCH_ESP32)
#error "sensors.cpp: Arduino Nano ESP32 only."
#endif

static Adafruit_SHT31 shtSensor;
static bool shtConnected = false;
static unsigned long lastShtReconnectAttempt = 0;
static uint8_t shtReconnectBackoffStep = 0;

int16_t tDS = SENSOR_INVALID;
int16_t tSHT = SENSOR_INVALID;
int16_t hSHT = SENSOR_INVALID;

SensorState ds18State = {false, 0, 0, false};
SensorState shtTempState = {false, 0, 0, false};
SensorState shtHumState = {false, 0, 0, false};

bool dsError = false;
bool tempError = false;
bool humError = false;

DallasTemperature* ds18 = nullptr;
bool ds18Present = false;

static OneWire* s_oneWire = nullptr;
static DallasTemperature* s_ds18 = nullptr;
static unsigned long ds18SettleDeadline = 0;
static bool ds18ReconnectActive = false;

// --- DS18B20 (INC temperature, OneWire) ---

static uint8_t sensors_ds18BusPin() {
  int gpio = digitalPinToGPIONumber(DS18_PIN);
  return (gpio >= 0) ? (uint8_t)gpio : (uint8_t)DS18_PIN;
}

static void sensors_ds18PrepareBus() {
  uint8_t busPin = sensors_ds18BusPin();
  static OneWire owInstance(busPin);
  static DallasTemperature dsInstance(&owInstance);
  s_oneWire = &owInstance;
  s_ds18 = &dsInstance;
  ds18 = &dsInstance;
  ds18Present = false;

  pinMode(DS18_PIN, INPUT_PULLUP);

  dsInstance.begin();
  dsInstance.setWaitForConversion(false);
  dsInstance.setResolution(10);
  ds18SettleDeadline = millis() + 100UL;
}

static bool sensors_ds18FinishProbe() {
  if (s_ds18 == nullptr) {
    return false;
  }

  uint8_t count = s_ds18->getDeviceCount();
#if DEBUG_SERIAL
  Serial.print(F("DS18B20 count: "));
  Serial.println(count);
#endif

  if (count == 0) {
    return false;
  }

  DeviceAddress addr;
  if (s_ds18->getAddress(addr, 0)) {
    s_ds18->setResolution(addr, 10);
#if DEBUG_SERIAL
    Serial.print(F("DS18 addr: "));
    for (uint8_t i = 0; i < 8; i++) {
      if (addr[i] < 16) {
        Serial.print('0');
      }
      Serial.print(addr[i], HEX);
    }
    Serial.println();
#endif
  }

  ds18Present = true;
  return true;
}

bool sensors_initDs18() {
  uint8_t busPin = sensors_ds18BusPin();
#if DEBUG_SERIAL
  Serial.print(F("DS18 pin "));
  Serial.print(DS18_PIN);
  Serial.print(F(" -> GPIO "));
  Serial.println(busPin);
#endif

  sensors_ds18PrepareBus();
  while (millis() < ds18SettleDeadline) {
    yield();
  }
  return sensors_ds18FinishProbe();
}

// --- FS400-SHT30 (GRH temperature and humidity, I2C) ---

bool sensors_initSht30() {
  shtConnected = shtSensor.begin(SHT30_I2C_ADDR);
  if (!shtConnected) {
    shtTempState.active = false;
    shtTempState.dataValid = false;
    shtHumState.active = false;
    shtHumState.dataValid = false;
  }
  return shtConnected;
}

bool sensors_isShtConnected() {
  return shtConnected;
}

void sensors_reinitSht30() {
  shtConnected = shtSensor.begin(SHT30_I2C_ADDR);
  if (!shtConnected) {
    shtTempState.active = false;
    shtTempState.dataValid = false;
    shtHumState.active = false;
    shtHumState.dataValid = false;
  }
}

bool sensors_pollDs18Reconnect(unsigned long nowMillis) {
  if (ds18Present) {
    ds18ReconnectActive = false;
    return false;
  }

  if (!ds18ReconnectActive) {
    sensors_ds18PrepareBus();
    ds18ReconnectActive = true;
    return false;
  }

  if (nowMillis < ds18SettleDeadline) {
    return false;
  }

  ds18ReconnectActive = false;
  return sensors_ds18FinishProbe();
}

bool sensors_ds18ConversionReady() {
  if (!ds18Present || ds18 == nullptr) {
    return false;
  }
  return ds18->isConversionComplete();
}

// Reading validation and smoothing.

bool sensor_validateTemp(int16_t temp) {
  return (temp >= TEMP_MIN && temp <= TEMP_MAX);
}

bool sensor_validateHum(int16_t hum) {
  return (hum >= HUM_MIN && hum <= HUM_MAX);
}

int16_t sensor_smoothEma(int16_t current, int16_t newSample) {
  if (current == SENSOR_INVALID) {
    return newSample;
  }
  return (int16_t)(((uint32_t)SMOOTH_ALPHA * (uint32_t)newSample +
                    (uint32_t)SMOOTH_BETA * (uint32_t)current) / 100U);
}

bool sensor_displayValueChanged(int16_t prev, int16_t next, int16_t minDelta) {
  if (prev == SENSOR_INVALID || next == SENSOR_INVALID) {
    return prev != next;
  }
  int16_t diff = next - prev;
  if (diff < 0) {
    diff = -diff;
  }
  return diff >= minDelta;
}

void sensor_updateState(SensorState &state, bool success) {
  unsigned long now = millis();

  if (success) {
    state.lastValidRead = now;
    state.errorCount = 0;
    state.dataValid = true;
    state.active = true;
  } else {
    state.errorCount++;
    if (state.errorCount >= SENSOR_ERROR_THRESHOLD) {
      state.dataValid = false;
      state.active = false;
    }
  }

  if (state.active && (now - state.lastValidRead > SENSOR_TIMEOUT)) {
    state.active = false;
    state.dataValid = false;
  }
}

void sensor_invalidateData(int16_t &value, SensorState &state) {
  if (!state.dataValid) {
    value = SENSOR_INVALID;
  }
}

// Sync tempError / humError / dsError with SensorState (logic, RGB, failsafe).
void sensor_updateErrorFlags() {
  tempError = !shtTempState.dataValid;
  humError = !shtHumState.dataValid;
  dsError = !ds18State.dataValid;
}

static void sensor_requestRedrawIfChanged(bool wasValid, bool isValid, int16_t prev, int16_t next,
                                          int16_t minDelta) {
  if (wasValid != isValid || sensor_displayValueChanged(prev, next, minDelta)) {
    app_markMainScreenDirty();
  }
}

static void sensor_applyReading(int16_t &value, SensorState &state, int16_t sample, bool sampleValid,
                                int16_t displayDelta) {
  const bool wasValid = state.dataValid;
  const int16_t prev = value;

  if (sampleValid) {
    value = sensor_smoothEma(value, sample);
    sensor_updateState(state, true);
  } else {
    sensor_updateState(state, false);
    sensor_invalidateData(value, state);
  }

  sensor_requestRedrawIfChanged(wasValid, state.dataValid, prev, value, displayDelta);
}

static void sensors_invalidateShtPair() {
  sensor_updateState(shtTempState, false);
  sensor_invalidateData(tSHT, shtTempState);
  sensor_updateState(shtHumState, false);
  sensor_invalidateData(hSHT, shtHumState);
}

static void sensors_pollDs18(unsigned long nowMillis) {
  if (!ds18ConvInProgress && ds18Present && ds18 != nullptr &&
      millisIntervalElapsed(nowMillis, lastDs18RequestMs, READ_INTERVAL)) {
    ds18->requestTemperatures();
    ds18ConvStart = nowMillis;
    ds18ConvInProgress = true;
    lastDs18RequestMs = nowMillis;
  }

  if (ds18ConvInProgress && ds18 != nullptr &&
      (sensors_ds18ConversionReady() ||
       millisIntervalElapsed(nowMillis, ds18ConvStart, DS18_CONV_TIME))) {
    const float rawDS = ds18->getTempCByIndex(0);

    if (rawDS != DEVICE_DISCONNECTED_C && !isnan(rawDS)) {
      const int16_t newTemp = (int16_t)(rawDS * 10);
      sensor_applyReading(tDS, ds18State, newTemp, sensor_validateTemp(newTemp), DISPLAY_TEMP_DELTA);
    } else {
      const bool wasValid = ds18State.dataValid;
      const int16_t prevT = tDS;
      sensor_updateState(ds18State, false);
      sensor_invalidateData(tDS, ds18State);
      sensor_requestRedrawIfChanged(wasValid, ds18State.dataValid, prevT, tDS, DISPLAY_TEMP_DELTA);
    }

    ds18ConvInProgress = false;
    lastDs18RequestMs = nowMillis;
    if (!ds18State.dataValid) {
      ds18Present = false;
      ds18ReconnectBackoffStep = 0;
      lastDs18ReconnectMs = nowMillis;
    }
  }

  if (!ds18Present &&
      millisIntervalElapsed(nowMillis, lastDs18ReconnectMs,
                            sensorReconnectBackoffMs(ds18ReconnectBackoffStep))) {
    lastDs18ReconnectMs = nowMillis;
    if (sensors_pollDs18Reconnect(nowMillis)) {
      ds18ReconnectBackoffStep = 0;
      ds18State.active = true;
      ds18State.errorCount = 0;
      ds18State.dataValid = false;
      app_log(F("DS18B20 reconnected"));
      app_markMainScreenDirty();
    } else {
      if (ds18ReconnectBackoffStep < SENSOR_RECONNECT_BACKOFF_STEPS - 1) {
        ds18ReconnectBackoffStep++;
      }
      sensor_updateState(ds18State, false);
      sensor_invalidateData(tDS, ds18State);
    }
  }
}

static void sensors_pollSht30(unsigned long nowMillis) {
  if (!shtConnected) {
    if (!millisIntervalElapsed(nowMillis, lastShtReconnectAttempt,
                               sensorReconnectBackoffMs(shtReconnectBackoffStep))) {
      sensors_invalidateShtPair();
      return;
    }
    lastShtReconnectAttempt = nowMillis;
    shtConnected = shtSensor.begin(SHT30_I2C_ADDR);
    if (shtConnected) {
      shtReconnectBackoffStep = 0;
    } else if (shtReconnectBackoffStep < SENSOR_RECONNECT_BACKOFF_STEPS - 1) {
      shtReconnectBackoffStep++;
    }
    sensors_invalidateShtPair();
    return;
  }

  if (!millisIntervalElapsed(nowMillis, lastShtReadMs, READ_INTERVAL)) {
    return;
  }

  app_watchdogReset();
  float rawSHT_T = NAN;
  float rawSHT_H = NAN;
  const bool shtReadOk = shtSensor.readBoth(&rawSHT_T, &rawSHT_H);

  if (shtReadOk && !isnan(rawSHT_T)) {
    const int16_t newTemp = (int16_t)(rawSHT_T * 10);
    sensor_applyReading(tSHT, shtTempState, newTemp, sensor_validateTemp(newTemp), DISPLAY_TEMP_DELTA);
  } else {
    const bool tempWasValid = shtTempState.dataValid;
    const int16_t prevT = tSHT;
    sensor_updateState(shtTempState, false);
    sensor_invalidateData(tSHT, shtTempState);
    sensor_requestRedrawIfChanged(tempWasValid, shtTempState.dataValid, prevT, tSHT, DISPLAY_TEMP_DELTA);
  }

  if (shtReadOk && !isnan(rawSHT_H)) {
    const int16_t newHum = (int16_t)rawSHT_H;
    sensor_applyReading(hSHT, shtHumState, newHum, sensor_validateHum(newHum), DISPLAY_HUM_DELTA);
  } else {
    const bool humWasValid = shtHumState.dataValid;
    const int16_t prevH = hSHT;
    sensor_updateState(shtHumState, false);
    sensor_invalidateData(hSHT, shtHumState);
    sensor_requestRedrawIfChanged(humWasValid, shtHumState.dataValid, prevH, hSHT, DISPLAY_HUM_DELTA);
  }

  if (!shtReadOk) {
    shtConnected = false;
    shtReconnectBackoffStep = 0;
    lastShtReconnectAttempt = nowMillis;
  }

  lastShtReadMs = nowMillis;
}

// Periodic poll of both sensors (called from main loop).
void sensors_poll(unsigned long nowMillis) {
  sensors_pollDs18(nowMillis);
  sensors_pollSht30(nowMillis);
}
