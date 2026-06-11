/*
 * Psi_Unit_Firmware.ino — Arduino entry point
 *
 * Minimal file: setup() runs once at power-on, then loop() repeats forever.
 * All work is delegated to app_setup / app_loop.
 */

#include "app_setup.h"
#include "app_loop.h"

void setup() {
  app_setup();
}

void loop() {
  app_loop();
}
