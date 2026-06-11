/*
 * app_state.cpp — shared application state definitions
 */

#include "app_state.h"

unsigned long lastShtReadMs = 0;
unsigned long lastDs18RequestMs = 0;
unsigned long lastDs18ReconnectMs = 0;
unsigned long lastDisplay = 0;
unsigned long lastClockRedraw = 0;
unsigned long lastActivity = 0;
unsigned long sleepTimeout = 120000UL;

bool isSleep = false;

bool splashActive = true;
unsigned long splashStartMillis = 0;

unsigned long ds18ConvStart = 0;
bool ds18ConvInProgress = false;

uint8_t ds18ReconnectBackoffStep = 0;

void app_touchActivity(unsigned long nowMillis) {
  lastActivity = nowMillis;
}
