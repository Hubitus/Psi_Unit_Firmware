/*
 * rgb_status.h — onboard RGB indicator on Arduino Nano ESP32
 *
 * Built-in RGB LED reflects controller state:
 *   RGB_MODE_OFF   — indication disabled in settings
 *   RGB_MODE_OK    — sensors healthy, green breathing
 *   RGB_MODE_WIFI  — Wi‑Fi access point active, blue breathing
 *   RGB_MODE_ERROR — sensor fault, orange-red cycle
 *
 * Envelope patterns and PWM are implemented in rgb_status.cpp.
 * While the OLED is awake (!isSleep) the LED stays steady; when the display
 * sleeps, breathing resumes (all modes including ERROR).
 */

#ifndef RGB_STATUS_H
#define RGB_STATUS_H

#include <Arduino.h>

enum RgbStatusMode : uint8_t {
  RGB_MODE_OFF = 0,
  RGB_MODE_OK,
  RGB_MODE_WIFI,
  RGB_MODE_ERROR
};

void rgb_status_begin();
void rgb_status_poll(unsigned long nowMillis);
void rgb_status_applySettings();
void rgb_status_setBrightnessPreview(uint8_t raw, bool active);
RgbStatusMode rgb_status_getMode();

#endif // RGB_STATUS_H
