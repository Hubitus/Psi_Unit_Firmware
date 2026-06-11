#ifndef INPUT_BACKEND_SIM_H
#define INPUT_BACKEND_SIM_H

#include "input_backend.h"

void sim_input_clear();
void sim_input_post(InputEventType type, int16_t delta = 0);

#endif
