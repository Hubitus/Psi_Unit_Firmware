/*
 * app_platform.h — ESP32 platform base services
 *
 * Safe GPIO state at startup, Serial logging, watchdog timer (WDT),
 * light brightness PWM, and peripheral re-initialization after I2C failure.
 */

#ifndef APP_PLATFORM_H
#define APP_PLATFORM_H

#include <Arduino.h>

void app_forceOutputsOff();
void app_serialPollHost();
void app_confirmBootOk();
void app_serialBegin();
void app_logBootInfo();
void app_log(const __FlashStringHelper *msg);
void app_watchdogEnable();
void app_watchdogReset();
void app_pollHeapWarning(unsigned long nowMillis);
void app_initLightPwm();
void app_setLightPwm(uint8_t duty);
void app_reinitPeripherals();

#endif // APP_PLATFORM_H
