/*
 * wifi_qr.h — Wi-Fi join QR code on the OLED screen
 *
 * Draws a QR with network data (SSID and password) using the QRCode library (Richard Moore).
 * Can also generate SVG for the web page.
 */

#ifndef WIFI_QR_H
#define WIFI_QR_H

#include <Arduino.h>

bool wifi_qr_draw(uint8_t x, uint8_t y, uint8_t pixelSize, const char *payload);
bool wifi_qr_drawFullscreen(const char *payload, uint8_t *outPixelSize);
bool wifi_qr_buildSvg(const char *payload, char *out, size_t cap, uint8_t modulePx, int *outLen);

#endif // WIFI_QR_H
