/*
 * app_state.h — shared application state (timers, flags)
 */

#ifndef APP_STATE_H
#define APP_STATE_H

#include <Arduino.h>

extern unsigned long lastShtReadMs;
extern unsigned long lastDs18RequestMs;
extern unsigned long lastDs18ReconnectMs;
extern unsigned long lastDisplay;
extern unsigned long lastClockRedraw;
extern unsigned long lastActivity;
extern unsigned long sleepTimeout;

extern bool isSleep;

extern bool splashActive;
extern unsigned long splashStartMillis;

extern unsigned long ds18ConvStart;
extern bool ds18ConvInProgress;

extern uint8_t ds18ReconnectBackoffStep;

void app_touchActivity(unsigned long nowMillis);

#endif // APP_STATE_H
