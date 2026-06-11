/*
 * i2c_bus.cpp — I2C bus management (SDA/SCL)
 *
 * Connects display, FS400-SHT30 sensor, and DS3231 RTC on one bus. After several
 * consecutive failures, restarts Wire and triggers peripheral re-initialization.
 */

#include "i2c_bus.h"
#include "config.h"
#include <Wire.h>

#if !defined(ARDUINO_ARCH_ESP32)
#error "i2c_bus.cpp: Arduino Nano ESP32 only."
#endif

static I2cRtcStalePredicate s_rtcStalePredicate = nullptr;

static const uint32_t I2C_CLOCK_HZ = 100000UL;
static const uint8_t I2C_FAIL_THRESHOLD = 3;
static const unsigned long I2C_RECOVER_COOLDOWN_MS = 2000UL;
static const uint32_t I2C_TIMEOUT_MS = 50;

static bool oledPresent = false;
static bool rtcPresent = false;
static uint8_t oledAddr = OLED_I2C_ADDR;
static uint8_t failCount = 0;
static unsigned long lastHealthMs = 0;
static unsigned long lastRecoverMs = 0;
static void (*peripheralReinit)() = nullptr;

void i2c_setPeripheralReinit(void (*handler)(void)) {
  peripheralReinit = handler;
}

void i2c_setRtcStalePredicate(I2cRtcStalePredicate predicate) {
  s_rtcStalePredicate = predicate;
}

static void i2c_applyBusConfig() {
  Wire.setPins(I2C_SDA, I2C_SCL);
  Wire.begin(I2C_SDA, I2C_SCL, I2C_CLOCK_HZ);
  Wire.setTimeOut(I2C_TIMEOUT_MS);
  Wire.setBufferSize(128);
}

void i2c_init(bool verboseLog) {
  Wire.end();
  delay(10);
  i2c_applyBusConfig();
  if (verboseLog) {
    Serial.print(F("Wire.setPins(A4,A5): "));
    Serial.println(F("OK"));
    Serial.print(F("Wire.begin(100kHz, tmo="));
    Serial.print(I2C_TIMEOUT_MS);
    Serial.println(F("ms): OK"));
  }
  delay(50);
  failCount = 0;
  lastHealthMs = millis();
}

void i2c_softReinit() {
  i2c_applyBusConfig();
}

bool i2c_isRtcPresent() {
  return rtcPresent;
}

bool i2c_probe(uint8_t addr, uint8_t *errOut) {
  Wire.beginTransmission(addr);
  uint8_t err = Wire.endTransmission();
  if (errOut != nullptr) {
    *errOut = err;
  }
  i2c_noteTransmission(err);
  return err == 0;
}

void i2c_noteTransmission(uint8_t err) {
  if (err == 0) {
    if (failCount > 0) {
      failCount--;
    }
    return;
  }
  if (failCount < 255) {
    failCount++;
  }
}

bool i2c_isHealthy() {
  return failCount < I2C_FAIL_THRESHOLD;
}

uint8_t i2c_failureCount() {
  return failCount;
}

bool i2c_isOledPresent() {
  return oledPresent;
}

uint8_t i2c_getOledAddress() {
  return oledAddr;
}

// Probe for OLED (0x3C or 0x3D), SHT30, and RTC on the bus at startup or after recovery.
bool i2c_probeDevices(bool verboseLog) {
  uint8_t err = 0;
  oledPresent = false;
  rtcPresent = false;
  oledAddr = OLED_I2C_ADDR;

  if (i2c_probe(OLED_I2C_ADDR, &err)) {
    oledPresent = true;
    oledAddr = OLED_I2C_ADDR;
    if (verboseLog) {
      Serial.println(F("I2C OLED 0x3C OK"));
    }
  } else if (i2c_probe(0x3D, &err)) {
    oledPresent = true;
    oledAddr = 0x3D;
    if (verboseLog) {
      Serial.println(F("I2C OLED 0x3D OK"));
    }
  } else if (verboseLog) {
    Serial.print(F("I2C OLED missing, err="));
    Serial.println(err);
  }

  if (i2c_probe(SHT30_I2C_ADDR, &err)) {
    if (verboseLog) {
      Serial.println(F("I2C SHT30 OK"));
    }
  } else if (verboseLog) {
    Serial.print(F("I2C SHT30 missing, err="));
    Serial.println(err);
  }

  rtcPresent = i2c_probe(RTC_I2C_ADDR, &err);
  if (rtcPresent) {
    if (verboseLog) {
      Serial.println(F("I2C RTC OK"));
    }
  } else if (verboseLog) {
    Serial.print(F("I2C RTC missing, err="));
    Serial.println(err);
  }

  return oledPresent;
}

void i2c_recover() {
  Serial.println(F("I2C bus recovery"));
  Wire.end();
  delay(10);
  i2c_applyBusConfig();
  delay(50);
  failCount = 0;
  lastRecoverMs = millis();
  i2c_probeDevices(false);
  if (peripheralReinit != nullptr) {
    peripheralReinit();
  }
}

// deferRecover=true: skip OLED probe/recover trigger for display only; SHT/RTC still recovered.
void i2c_poll(unsigned long nowMillis, bool deferRecover) {
  if (nowMillis - lastHealthMs < I2C_HEALTH_INTERVAL_MS) {
    return;
  }
  lastHealthMs = nowMillis;

  bool oledOk = true;
  if (oledPresent && !deferRecover) {
    oledOk = i2c_probe(oledAddr);
  }
  const bool shtOk = i2c_probe(SHT30_I2C_ADDR);
  const bool rtcOk = i2c_probe(RTC_I2C_ADDR);

  if (oledOk && shtOk && rtcOk) {
    failCount = 0;
    return;
  }

  if (failCount < I2C_FAIL_THRESHOLD || (nowMillis - lastRecoverMs < I2C_RECOVER_COOLDOWN_MS)) {
    return;
  }

  const bool oledBad = oledPresent && !deferRecover && !oledOk;
  const bool rtcStale = !rtcOk && s_rtcStalePredicate != nullptr && s_rtcStalePredicate();
  if (oledBad || !shtOk || rtcStale) {
    i2c_recover();
  }
}
