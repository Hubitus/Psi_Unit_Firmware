/*
 * logic.cpp — actuator on/off control logic
 *
 * Uses sensor readings and user settings to decide when to enable heat,
 * humidification, ventilation, lighting, and INC. Drives physical GPIO outputs
 * (HIGH = on). Holds all outputs off briefly at boot.
 */

#include "logic.h"
#include "storage.h"
#include "sensors.h"
#include "rtc_clock.h"
#include "app_snapshot.h"
#include "app_platform.h"
#include <Arduino.h>

bool heatOn = false;
bool humOn = false;
bool fanOn = false;
bool lightOn = false;
bool incOn = false;

unsigned long fanCycleMs = 0;
unsigned long humCycleMs = 0;
unsigned long humMaxStartMs = 0;
unsigned long heatMaxStartMs = 0;
unsigned long incMaxStartMs = 0;
bool fanPhase = false;
bool humPhase = false;

// Temporary light on from main screen (long button press).
static bool lightOverrideActive = false;
static unsigned long lightOverrideStartMs = 0;

struct CycleCache {
  uint8_t workTime;
  uint8_t restTime;
  uint8_t workUnit;
  uint8_t restUnit;
  uint16_t workSec;
  uint16_t restSec;
};

static CycleCache humCycleCache = {0, 0, 0xFF, 0xFF, 0, 0};
static CycleCache fanCycleCache = {0, 0, 0xFF, 0xFF, 0, 0};

// Post-boot delay: outputs stay off until timer expires.
static bool outputsEnabled = false;
static unsigned long outputsEnableAtMs = 0;

struct OutputPinState {
  bool heat;
  bool hum;
  bool inc;
  bool fan;
  uint8_t lightPwm;
  bool synced;
};

static OutputPinState pinsApplied = {false, false, false, false, 0xFF, false};

static void logic_invalidateOutputPins() {
  pinsApplied.synced = false;
}

static void logic_pinsAllLow() {
  if (pinsApplied.synced && !pinsApplied.heat && !pinsApplied.hum && !pinsApplied.inc &&
      !pinsApplied.fan && pinsApplied.lightPwm == 0) {
    return;
  }
  digitalWrite(SSR_HEAT, LOW);
  digitalWrite(SSR_HUM, LOW);
  digitalWrite(SSR_INC, LOW);
  digitalWrite(RELAY_FAN, LOW);
  app_setLightPwm(0);
  pinsApplied.heat = false;
  pinsApplied.hum = false;
  pinsApplied.inc = false;
  pinsApplied.fan = false;
  pinsApplied.lightPwm = 0;
  pinsApplied.synced = true;
}

// Write current relay and light PWM state to physical pins.
void logic_applyOutputPins() {
  if (!outputsEnabled) {
    logic_pinsAllLow();
    return;
  }

  const bool forceSync = !pinsApplied.synced;

  if (forceSync || heatOn != pinsApplied.heat) {
    digitalWrite(SSR_HEAT, heatOn ? HIGH : LOW);
    pinsApplied.heat = heatOn;
  }
  if (forceSync || humOn != pinsApplied.hum) {
    digitalWrite(SSR_HUM, humOn ? HIGH : LOW);
    pinsApplied.hum = humOn;
  }
  if (forceSync || incOn != pinsApplied.inc) {
    digitalWrite(SSR_INC, incOn ? HIGH : LOW);
    pinsApplied.inc = incOn;
  }
  if (forceSync || fanOn != pinsApplied.fan) {
    digitalWrite(RELAY_FAN, fanOn ? HIGH : LOW);
    pinsApplied.fan = fanOn;
  }

  uint8_t pwmDuty = 0;
  if (lightOn) {
    pwmDuty = (uint8_t)(((uint16_t)settings.lightBrightness * 255U + 50U) / 100U);
  }
  if (forceSync || pwmDuty != pinsApplied.lightPwm) {
    app_setLightPwm(pwmDuty);
    pinsApplied.lightPwm = pwmDuty;
  }
  pinsApplied.synced = true;
}

void logic_beginBootHold() {
  outputsEnabled = false;
  outputsEnableAtMs = millis() + OUTPUT_BOOT_HOLD_MS;
  logic_invalidateOutputPins();
  logic_pinsAllLow();
}

void logic_pollBootHold(unsigned long nowMillis) {
  if (!outputsEnabled && millisDeadlineReached(nowMillis, outputsEnableAtMs)) {
    outputsEnabled = true;
    logic_invalidateOutputPins();
  }
  if (!outputsEnabled) {
    logic_pinsAllLow();
  }
}

static uint16_t logic_toSeconds(uint8_t value, TimeUnit unit) {
  return (unit == UNIT_MINUTES) ? (uint16_t)value * 60U : (uint16_t)value;
}

static void logic_updateCycleCache(CycleCache &cache, uint8_t workTime, uint8_t restTime, TimeUnit workUnit,
                                   TimeUnit restUnit, bool &phase, unsigned long &phaseStartMs, unsigned long nowMs) {
  uint8_t workUnitRaw = (uint8_t)workUnit;
  uint8_t restUnitRaw = (uint8_t)restUnit;
  if (cache.workTime == workTime && cache.restTime == restTime && cache.workUnit == workUnitRaw &&
      cache.restUnit == restUnitRaw) {
    return;
  }
  cache.workTime = workTime;
  cache.restTime = restTime;
  cache.workUnit = workUnitRaw;
  cache.restUnit = restUnitRaw;
  cache.workSec = logic_toSeconds(workTime, workUnit);
  cache.restSec = logic_toSeconds(restTime, restUnit);
  phase = true;
  phaseStartMs = nowMs;
}

static void logic_controlCycleMillis(bool &deviceOn, bool &phase, unsigned long &phaseStartMs,
                                   uint16_t workSec, uint16_t restSec, unsigned long nowMs) {
  unsigned long elapsedSec = (nowMs - phaseStartMs) / 1000UL;

  if (phase) {
    if (elapsedSec >= workSec) {
      phase = false;
      phaseStartMs = nowMs;
      deviceOn = false;
    } else {
      deviceOn = true;
    }
  } else {
    if (elapsedSec >= restSec) {
      phase = true;
      phaseStartMs = nowMs;
      deviceOn = true;
    } else {
      deviceOn = false;
    }
  }
}

static bool logic_hysteresisOn(int16_t value, int16_t target, int16_t hyst, bool currentlyOn) {
  const int16_t threshold = target - hyst;
  if (value < threshold) {
    return true;
  }
  if (value >= target) {
    return false;
  }
  return currentlyOn;
}

static void logic_setOutputState(bool &output, bool shouldBeOn) {
  if (shouldBeOn != output) {
    output = shouldBeOn;
    app_markMainScreenDirty();
  }
}

static bool logic_tempOverSafetyMax(int16_t tempDeci) {
  return TEMP_SAFETY_MAX > 0 && sensor_hasValue(tempDeci) && tempDeci >= TEMP_SAFETY_MAX;
}

static bool logic_incOverSafetyMax(int16_t tempDeci) {
  return INC_SAFETY_MAX > 0 && sensor_hasValue(tempDeci) && tempDeci >= INC_SAFETY_MAX;
}

static void logic_applyMaxOnLimit(bool shouldBeOn, bool &deviceOn, unsigned long &maxStartMs, uint16_t maxOnMin,
                                  unsigned long nowMillis, bool &outShouldBeOn) {
  if (!shouldBeOn || maxOnMin == 0) {
    return;
  }
  if (!deviceOn) {
    maxStartMs = nowMillis;
    return;
  }
  const unsigned long elapsedSec = (nowMillis - maxStartMs) / 1000UL;
  if (elapsedSec >= (unsigned long)maxOnMin * 60UL) {
    outShouldBeOn = false;
  }
}

bool logic_isCriticalFault() {
  return !storage_isReady() || (tempError && humError && dsError);
}

static bool logic_hasNegativeTempReading() {
  return (sensor_hasValue(tSHT) && tSHT < 0) || (sensor_hasValue(tDS) && tDS < 0);
}

static void logic_forceAllOutputsOff() {
  if (lightOverrideActive) {
    lightOverrideActive = false;
    app_markMainScreenDirty();
  }
  logic_setOutputState(heatOn, false);
  logic_setOutputState(humOn, false);
  logic_setOutputState(fanOn, false);
  logic_setOutputState(lightOn, false);
  logic_setOutputState(incOn, false);
  logic_invalidateOutputPins();
}

// No NVS or all sensors ERR — all outputs OFF; channel automation skipped.
void logic_onCriticalFault() {
  if (!logic_isCriticalFault()) {
    return;
  }
  logic_forceAllOutputsOff();
}

bool logic_isNegativeTempSafety() {
  return logic_hasNegativeTempReading();
}

// Valid reading below 0 °C — all outputs OFF (safety).
void logic_onNegativeTempSafety() {
  if (!logic_hasNegativeTempReading()) {
    return;
  }
  logic_forceAllOutputsOff();
}

// After channel logic: force SSRs off on sensor errors; clear light override.
void logic_applyFailsafe(unsigned long nowMillis) {
  (void)nowMillis;

  if (lightOverrideActive && (tempError || humError)) {
    lightOverrideActive = false;
    logic_controlLight();
  }

  if (tempError && heatOn) {
    logic_setOutputState(heatOn, false);
  }
  if (humError && humOn) {
    logic_setOutputState(humOn, false);
  }
  if (dsError && incOn) {
    logic_setOutputState(incOn, false);
  }
}

// Heat: AUTO from SHT30 (GRH) temperature with hysteresis, or forced mode.
void logic_controlHeat(unsigned long nowMillis) {
  bool shouldBeOn = false;

  if (settings.heatMode == MODE_FORCE_ON) {
    shouldBeOn = !tempError;
  } else if (settings.heatMode == MODE_FORCE_OFF) {
    shouldBeOn = false;
  } else if (settings.heatMode == MODE_AUTO) {
    if (!tempError && sensor_hasValue(tSHT)) {
      shouldBeOn = logic_hysteresisOn(tSHT, settings.tTarget, settings.tHyst, heatOn);
    }
  }

  if (logic_tempOverSafetyMax(tSHT)) {
    shouldBeOn = false;
  }
  logic_applyMaxOnLimit(shouldBeOn, heatOn, heatMaxStartMs, HEAT_MAX_ON_MIN, nowMillis, shouldBeOn);

  logic_setOutputState(heatOn, shouldBeOn);
}

// Humidifier: AUTO by humidity, max run-time limit, or MANUAL cycle.
void logic_controlHum(unsigned long nowMillis) {
  bool shouldBeOn = false;

  if (settings.humMode == MODE_FORCE_ON) {
    shouldBeOn = !humError;
  } else if (settings.humMode == MODE_FORCE_OFF) {
    shouldBeOn = false;
  } else if (settings.humMode == MODE_AUTO) {
    if (!humError) {
      shouldBeOn = logic_hysteresisOn(hSHT, settings.hTarget, settings.hHyst, humOn);

      if (shouldBeOn && humOn) {
        unsigned long elapsedSec = (nowMillis - humMaxStartMs) / 1000UL;
        if (elapsedSec >= (unsigned long)settings.humMax * 60UL) {
          shouldBeOn = false;
        }
      }

      if (shouldBeOn && !humOn) {
        humMaxStartMs = nowMillis;
      }
    }
  } else if (settings.humMode == MODE_MANUAL) {
    if (humError) {
      shouldBeOn = false;
    } else {
      logic_updateCycleCache(humCycleCache, settings.humWork, settings.humRest, (TimeUnit)settings.humWorkUnit,
                             (TimeUnit)settings.humRestUnit, humPhase, humCycleMs, nowMillis);
      logic_controlCycleMillis(shouldBeOn, humPhase, humCycleMs, humCycleCache.workSec, humCycleCache.restSec,
                               nowMillis);
    }
  }

  if (logic_tempOverSafetyMax(tSHT)) {
    shouldBeOn = false;
  }

  logic_setOutputState(humOn, shouldBeOn);
}

// Fan: forced mode or work/rest cycle in AUTO.
void logic_controlFan(unsigned long nowMillis) {
  bool shouldBeOn = false;

  if (settings.fanMode == MODE_FORCE_ON) {
    shouldBeOn = true;
  } else if (settings.fanMode == MODE_FORCE_OFF) {
    shouldBeOn = false;
  } else if (settings.fanMode == MODE_AUTO) {
    logic_updateCycleCache(fanCycleCache, settings.fanWork, settings.fanRest, (TimeUnit)settings.fanWorkUnit,
                           (TimeUnit)settings.fanRestUnit, fanPhase, fanCycleMs, nowMillis);
    logic_controlCycleMillis(shouldBeOn, fanPhase, fanCycleMs, fanCycleCache.workSec, fanCycleCache.restSec,
                             nowMillis);
  }

  logic_setOutputState(fanOn, shouldBeOn);
}

bool logic_isLightOverrideActive() {
  return lightOverrideActive;
}

void logic_toggleLightOverride() {
  lightOverrideActive = !lightOverrideActive;
  if (lightOverrideActive) {
    lightOverrideStartMs = millis();
  }
  logic_controlLight();
}

// Light: RTC schedule, forced mode, or temporary override.
void logic_controlLight() {
  if (lightOverrideActive && LIGHT_OVERRIDE_MAX_MIN > 0) {
    const unsigned long elapsedMin = (millis() - lightOverrideStartMs) / 60000UL;
    if (elapsedMin >= LIGHT_OVERRIDE_MAX_MIN) {
      lightOverrideActive = false;
      app_markMainScreenDirty();
    }
  }

  bool shouldBeOn = false;

  if (lightOverrideActive) {
    shouldBeOn = true;
  } else if (settings.lightMode == MODE_FORCE_ON) {
    shouldBeOn = true;
  } else if (settings.lightMode == MODE_FORCE_OFF) {
    shouldBeOn = false;
  } else if (settings.lightMode == MODE_AUTO) {
    if (rtc_hasValidNow()) {
      const DateTime &now = rtc_getNow();
      uint8_t h = now.hour();
      uint8_t endHour = (settings.lightHour + settings.lightDuration) % 24;

      if (settings.lightHour < endHour) {
        shouldBeOn = (h >= settings.lightHour && h < endHour);
      } else {
        shouldBeOn = (h >= settings.lightHour || h < endHour);
      }
    }
  }

  logic_setOutputState(lightOn, shouldBeOn);
}

// INC (substrate heat): AUTO from DS18B20 with hysteresis.
void logic_controlInc(unsigned long nowMillis) {
  bool shouldBeOn = false;

  if (settings.incMode == MODE_FORCE_ON) {
    shouldBeOn = !dsError;
  } else if (settings.incMode == MODE_FORCE_OFF) {
    shouldBeOn = false;
  } else if (settings.incMode == MODE_AUTO) {
    if (!dsError && sensor_hasValue(tDS)) {
      shouldBeOn = logic_hysteresisOn(tDS, settings.incTarget, settings.incHyst, incOn);
    }
  }

  if (logic_incOverSafetyMax(tDS)) {
    shouldBeOn = false;
  }
  logic_applyMaxOnLimit(shouldBeOn, incOn, incMaxStartMs, INC_MAX_ON_MIN, nowMillis, shouldBeOn);

  logic_setOutputState(incOn, shouldBeOn);
}
