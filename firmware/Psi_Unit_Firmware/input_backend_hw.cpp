/*
 * input_backend_hw.cpp — button and encoder polling on Arduino Nano ESP32
 *
 * Uses the Encoder library. The encoder object is created only in
 * input_backend_begin(), not globally, so GPIO interrupts are not set up before setup().
 */

#include "input_backend.h"
#include "config.h"
#include <Encoder.h>

static Encoder *s_encoder = nullptr;

static long s_encPrev = 0;
static long s_encCurrent = 0;
static unsigned long s_lastEncRead = 0;

static bool s_buttonState = false;
static bool s_lastButtonReading = false;
static unsigned long s_lastButtonChange = 0;
static unsigned long s_btnPressTime = 0;
static bool s_btnPressed = false;
static bool s_longPress = false;

static bool input_backend_pushEvent(InputEvent *out, uint8_t maxEvents, uint8_t &count, InputEventType type,
                                    int16_t delta) {
  if (count >= maxEvents) {
    return false;
  }
  out[count].type = type;
  out[count].delta = delta;
  count++;
  return true;
}

// Button polling: debounce, short and long press.
static void input_backend_pollButton(unsigned long nowMillis, InputEvent *out, uint8_t maxEvents, uint8_t &count) {
  const bool reading = digitalRead(ENC_BTN) == LOW;

  if (reading != s_lastButtonReading) {
    s_lastButtonChange = nowMillis;
  }
  s_lastButtonReading = reading;

  if ((nowMillis - s_lastButtonChange) <= BTN_DEBOUNCE) {
    return;
  }

  if (reading != s_buttonState) {
    s_buttonState = reading;

    if (s_buttonState) {
      s_btnPressTime = nowMillis;
      s_btnPressed = false;
      s_longPress = false;
      input_backend_pushEvent(out, maxEvents, count, INPUT_WAKE, 0);
    } else {
      if (!s_btnPressed && !s_longPress) {
        s_btnPressed = true;
        input_backend_pushEvent(out, maxEvents, count, INPUT_SHORT_PRESS, 0);
      }
    }
  }

  if (s_buttonState && !s_longPress && (nowMillis - s_btnPressTime > LONG_PRESS_TIME)) {
    s_longPress = true;
    s_btnPressed = true;
    input_backend_pushEvent(out, maxEvents, count, INPUT_LONG_PRESS, 0);
  }
}

static void input_backend_pollEncoder(unsigned long nowMillis, InputEvent *out, uint8_t maxEvents, uint8_t &count) {
  if (s_encoder == nullptr || (nowMillis - s_lastEncRead) < ENC_READ_INTERVAL) {
    return;
  }

  s_encCurrent = s_encoder->read() / ENC_TICKS_PER_STEP;
  const int delta = (int)(s_encCurrent - s_encPrev);

  if (delta != 0) {
    s_encPrev = s_encCurrent;
    input_backend_pushEvent(out, maxEvents, count, INPUT_ENCODER, (int16_t)delta);
  }
  s_lastEncRead = nowMillis;
}

void input_backend_begin() {
  pinMode(ENC_BTN, INPUT_PULLUP);
  static Encoder encoderInst(ENC_B, ENC_A);
  s_encoder = &encoderInst;
  s_encPrev = s_encoder->read() / ENC_TICKS_PER_STEP;
  s_encCurrent = s_encPrev;
  s_lastEncRead = 0;
}

uint8_t input_backend_poll(unsigned long nowMillis, InputEvent *out, uint8_t maxEvents) {
  if (out == nullptr || maxEvents == 0) {
    return 0;
  }

  uint8_t count = 0;
  input_backend_pollButton(nowMillis, out, maxEvents, count);
  input_backend_pollEncoder(nowMillis, out, maxEvents, count);
  return count;
}
