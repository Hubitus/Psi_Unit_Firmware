#include "Arduino.h"
#include <chrono>

static std::chrono::steady_clock::time_point s_boot;

static std::chrono::steady_clock::time_point &bootTime() {
  static bool init = false;
  if (!init) {
    s_boot = std::chrono::steady_clock::now();
    init = true;
  }
  return s_boot;
}

unsigned long millis() {
  const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - bootTime());
  return (unsigned long)ms.count();
}

void delay(unsigned long ms) {
  (void)ms;
}

void pinMode(uint8_t, uint8_t) {}

int digitalRead(uint8_t) {
  return HIGH;
}

void digitalWrite(uint8_t, int) {}

class TwoWire {};

TwoWire Wire;

HardwareSerial Serial;
