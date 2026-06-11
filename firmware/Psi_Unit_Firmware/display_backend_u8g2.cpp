/*
 * display_backend_u8g2.cpp — SH1106 display driver via U8g2 library
 *
 * U8G2 is created lazily on first disp_oled() call, not globally:
 * early HW I2C init before setup() conflicts with i2c_init().
 */

#include "display_backend.h"
#include "config.h"
#include <U8g2lib.h>

#if !defined(ARDUINO_ARCH_ESP32)
#error "display_backend_u8g2.cpp: Arduino Nano ESP32 only."
#endif

static U8G2_SH1106_128X64_NONAME_1_HW_I2C *s_u8g2 = nullptr;

static U8G2_SH1106_128X64_NONAME_1_HW_I2C &disp_oled() {
  if (s_u8g2 == nullptr) {
    static U8G2_SH1106_128X64_NONAME_1_HW_I2C inst(U8G2_R0);
    s_u8g2 = &inst;
  }
  return *s_u8g2;
}

void disp_begin(uint8_t oledI2cAddr7bit) {
  U8G2_SH1106_128X64_NONAME_1_HW_I2C &oled = disp_oled();
  oled.setI2CAddress(oledI2cAddr7bit << 1);
  oled.setBusClock(100000);
  oled.begin();
}

void disp_wake() {
  disp_oled().setPowerSave(0);
}

void disp_setContrast(uint8_t raw) {
  disp_oled().setContrast(raw);
}

void disp_setInverted(bool inverted) {
  disp_oled().sendF("c", inverted ? (uint8_t)0xA7 : (uint8_t)0xA6);
}

void disp_setRotation180(bool rotated) {
  disp_oled().setDisplayRotation(rotated ? U8G2_R2 : U8G2_R0);
}

void disp_clear() {
  disp_oled().clearDisplay();
}

void disp_setPowerSave(bool enabled) {
  disp_oled().setPowerSave(enabled ? 1 : 0);
}

void disp_renderPages(DispDrawCallback drawFn) {
  if (drawFn == nullptr) {
    return;
  }
  U8G2_SH1106_128X64_NONAME_1_HW_I2C &oled = disp_oled();
  oled.firstPage();
  do {
    drawFn();
  } while (oled.nextPage());
}

void disp_setFontUi() {
  disp_oled().setFont(u8g2_font_6x10_tr);
  disp_oled().setFontMode(1);
  disp_oled().setFontPosBaseline();
}

void disp_setFontToast() {
  disp_oled().setFont(u8g2_font_6x10_tr);
  disp_oled().setFontMode(1);
  disp_oled().setFontPosBaseline();
}

void disp_setDrawColor(uint8_t color) {
  disp_oled().setDrawColor(color);
}

void disp_drawStr(uint8_t x, uint8_t y, const char *text) {
  disp_oled().drawStr(x, y, text);
}

void disp_drawStrP(uint8_t x, uint8_t y, PGM_P text) {
  disp_oled().drawStr(x, y, text);
}

void disp_drawStrCenteredInBox(uint8_t x, uint8_t y, uint8_t w, uint8_t h, const char *text) {
  U8G2_SH1106_128X64_NONAME_1_HW_I2C &oled = disp_oled();
  const uint8_t tw = (uint8_t)oled.getStrWidth(text);
  const uint8_t ascent = (uint8_t)oled.getAscent();
  const uint8_t descent = (uint8_t)oled.getDescent();
  const uint8_t tx = (uint8_t)(x + (w - tw) / 2);
  const uint8_t ty = (uint8_t)(y + (h + ascent - descent) / 2);
  oled.drawStr(tx, ty, text);
}

void disp_drawBox(uint8_t x, uint8_t y, uint8_t w, uint8_t h) {
  disp_oled().drawBox(x, y, w, h);
}

void disp_drawFrame(uint8_t x, uint8_t y, uint8_t w, uint8_t h) {
  disp_oled().drawFrame(x, y, w, h);
}

void disp_drawHLine(uint8_t x, uint8_t y, uint8_t w) {
  disp_oled().drawHLine(x, y, w);
}

void disp_drawPixel(uint8_t x, uint8_t y) {
  disp_oled().drawPixel(x, y);
}

void disp_drawDisc(uint8_t x, uint8_t y, uint8_t r) {
  disp_oled().drawDisc(x, y, r, U8G2_DRAW_ALL);
}

void disp_drawCircleArc(uint8_t x, uint8_t y, uint8_t r) {
  disp_oled().drawCircle(x, y, r, U8G2_DRAW_UPPER_LEFT | U8G2_DRAW_UPPER_RIGHT);
}

void disp_drawXBMP(uint8_t x, uint8_t y, uint8_t w, uint8_t h, const uint8_t *bitmap) {
  disp_oled().drawXBMP(x, y, w, h, bitmap);
}

uint8_t disp_getStrWidth(const char *text) {
  return disp_oled().getStrWidth(text);
}

uint8_t disp_getStrWidthP(PGM_P text) {
  return disp_oled().getStrWidth(text);
}

uint8_t disp_getFontAscent() {
  return (uint8_t)disp_oled().getAscent();
}

uint8_t disp_getFontDescent() {
  const int8_t descent = disp_oled().getDescent();
  return descent > 0 ? (uint8_t)descent : 0;
}

void disp_setClipWindow(uint8_t clipX0, uint8_t clipY0, uint8_t clipX1, uint8_t clipY1) {
  disp_oled().setClipWindow(clipX0, clipY0, clipX1, clipY1);
}

void disp_setMaxClipWindow() {
  disp_oled().setMaxClipWindow();
}
