/*
 * wifi_icon.h — Wi-Fi XBM icon for OLED (PROGMEM)
 *
 * 13×6 px, 1-bit, XBM LSB-first.
 */

#ifndef WIFI_ICON_H
#define WIFI_ICON_H

#include <Arduino.h>

const uint8_t WIFI_ICON_WIDTH = 13;
const uint8_t WIFI_ICON_HEIGHT = 6;
const unsigned char wifiIconBits[] PROGMEM = {
    // WiFi_Icon_13x6
    0x40, 0x16, 0x00, 0x02, 0x55, 0x17, 0x55, 0x12, 0x55, 0x12, 0x4a, 0x12
};

#endif // WIFI_ICON_H
