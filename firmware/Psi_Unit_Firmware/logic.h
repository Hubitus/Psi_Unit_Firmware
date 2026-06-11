/*
 * logic.h — heater, humidifier, fan, light, and INC control
 *
 * Declares functions that decide when to turn each output (relays and SSRs)
 * on or off per AUTO, forced ON/OFF, or work/rest cycle mode.
 * heatOn, fanOn, etc. are read by the display and web UI.
 */

#ifndef LOGIC_H
#define LOGIC_H

#include "config.h"
#include "types.h"

extern bool heatOn;
extern bool humOn;
extern bool fanOn;
extern bool lightOn;
extern bool incOn;
extern unsigned long fanCycleMs;
extern unsigned long humCycleMs;
extern unsigned long humMaxStartMs;
extern unsigned long heatMaxStartMs;
extern unsigned long incMaxStartMs;
extern bool fanPhase, humPhase;
void logic_beginBootHold();
void logic_pollBootHold(unsigned long nowMillis);
void logic_applyOutputPins();
void logic_applyFailsafe(unsigned long nowMillis);
bool logic_isCriticalFault();
void logic_onCriticalFault();
bool logic_isNegativeTempSafety();
void logic_onNegativeTempSafety();
void logic_controlHeat(unsigned long nowMillis);
void logic_controlHum(unsigned long nowMillis);
void logic_controlFan(unsigned long nowMillis);
void logic_controlLight();
void logic_toggleLightOverride();
bool logic_isLightOverrideActive();
void logic_controlInc(unsigned long nowMillis);

#endif // LOGIC_H
