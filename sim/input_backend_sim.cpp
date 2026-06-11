/*
 * input_backend_sim.cpp — InputEvent queue (fed from SDL in main.cpp)
 */

#include "input_backend.h"

static InputEvent s_queue[8];
static uint8_t s_count = 0;

void sim_input_clear() {
  s_count = 0;
}

void sim_input_post(InputEventType type, int16_t delta) {
  if (s_count >= 8) {
    return;
  }
  s_queue[s_count].type = type;
  s_queue[s_count].delta = delta;
  s_count++;
}

void input_backend_begin() {
  sim_input_clear();
}

uint8_t input_backend_poll(unsigned long, InputEvent *out, uint8_t maxEvents) {
  if (out == nullptr || maxEvents == 0) {
    return 0;
  }
  const uint8_t n = s_count < maxEvents ? s_count : maxEvents;
  for (uint8_t i = 0; i < n; i++) {
    out[i] = s_queue[i];
  }
  if (n < s_count) {
    for (uint8_t i = 0; i < s_count - n; i++) {
      s_queue[i] = s_queue[i + n];
    }
  }
  s_count = (uint8_t)(s_count - n);
  return n;
}
