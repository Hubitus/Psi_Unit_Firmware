/*
 * serial_cli.h — debug commands via Serial Monitor (USB, 115200 baud)
 *
 * Prints current settings, sensor readings, output state, and controls Wi-Fi
 * without the display. Useful for setup and diagnostics.
 */

#ifndef SERIAL_CLI_H
#define SERIAL_CLI_H

#include <Arduino.h>

void serial_cli_poll();

#endif // SERIAL_CLI_H
