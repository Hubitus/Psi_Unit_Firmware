/*
 * sim_platform.cpp — app_platform stubs for desktop
 */

#include "app_platform.h"
#include "config.h"

void app_forceOutputsOff() {}
void app_serialPollHost() {}
void app_confirmBootOk() {}
void app_serialBegin() {}

void app_logBootInfo() {
  Serial.println(F("Build: sim-desktop"));
  Serial.println(F("Features: snapshot,commands,display_backend,input_backend"));
}

void app_log(const __FlashStringHelper *msg) {
  Serial.println(msg);
}

void app_watchdogEnable() {}
void app_watchdogReset() {}
void app_initLightPwm() {}
void app_setLightPwm(uint8_t) {}
void app_reinitPeripherals() {}
