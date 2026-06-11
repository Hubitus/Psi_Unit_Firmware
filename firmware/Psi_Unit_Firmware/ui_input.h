/*
 * ui_input.h — on-screen button and encoder handling
 *
 * Converts presses (short, long, encoder rotation) into menu actions:
 * screen navigation, value changes, return to main screen.
 */

#ifndef UI_INPUT_H
#define UI_INPUT_H

#include <Arduino.h>
#include "input_backend.h"

void ui_input_pollInactivity(unsigned long nowMillis);
void ui_input_handleEvent(const InputEvent &ev);
void ui_input_onShortPress();
void ui_input_onLongPress();
void ui_input_onEncoder(int delta);

#endif // UI_INPUT_H
