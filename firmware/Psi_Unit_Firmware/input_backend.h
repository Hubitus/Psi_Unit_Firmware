/*
 * input_backend.h — hardware button and encoder
 *
 * Polls GPIO button (short/long press) and encoder. Returns an InputEvent queue
 * for ui_input. Hardware implementation is in input_backend_hw.cpp.
 */

#ifndef INPUT_BACKEND_H
#define INPUT_BACKEND_H

#include <Arduino.h>

enum InputEventType : uint8_t {
  INPUT_NONE = 0,
  INPUT_WAKE,
  INPUT_SHORT_PRESS,
  INPUT_LONG_PRESS,
  INPUT_ENCODER,
};

struct InputEvent {
  InputEventType type;
  int16_t delta;
};

void input_backend_begin();
uint8_t input_backend_poll(unsigned long nowMillis, InputEvent *out, uint8_t maxEvents);

#endif // INPUT_BACKEND_H
