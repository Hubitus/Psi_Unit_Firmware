/*
 * i2c_bus.h — shared I2C bus for OLED, FS400-SHT30, and RTC
 *
 * Wire initialization, bus device probing, error counting, and automatic
 * recovery on hang. Other modules report errors via i2c_noteTransmission.
 */

#ifndef I2C_BUS_H
#define I2C_BUS_H

#include <Arduino.h>

void i2c_init(bool verboseLog);
void i2c_softReinit();
bool i2c_probe(uint8_t addr, uint8_t *errOut = nullptr);
void i2c_noteTransmission(uint8_t err);
// deferRecover: defer OLED probe/recovery only (SHT30/RTC recovery still runs).
void i2c_poll(unsigned long nowMillis, bool deferRecover = false);
bool i2c_isHealthy();
uint8_t i2c_failureCount();

bool i2c_probeDevices(bool verboseLog);
bool i2c_isOledPresent();
uint8_t i2c_getOledAddress();
bool i2c_isRtcPresent();

void i2c_recover();
void i2c_setPeripheralReinit(void (*handler)(void));
typedef bool (*I2cRtcStalePredicate)(void);
void i2c_setRtcStalePredicate(I2cRtcStalePredicate predicate);

#endif // I2C_BUS_H
