/*
 * display_backend.h — drawing layer for monochrome OLED 128×64
 *
 * Unified interface for display_ui: fonts, lines, frames, bitmaps. Drawing
 * runs only inside the callback passed to disp_renderPages().
 */

#ifndef DISPLAY_BACKEND_H
#define DISPLAY_BACKEND_H

#include <Arduino.h>

#define DISP_WIDTH 128
#define DISP_HEIGHT 64

typedef void (*DispDrawCallback)(void);

void disp_begin(uint8_t oledI2cAddr7bit);
void disp_wake();
void disp_setContrast(uint8_t raw);
void disp_setInverted(bool inverted);
void disp_setRotation180(bool rotated);
void disp_clear();
void disp_setPowerSave(bool enabled);
void disp_renderPages(DispDrawCallback drawFn);

void disp_setFontUi();
void disp_setFontToast();
void disp_setDrawColor(uint8_t color);
void disp_drawStr(uint8_t x, uint8_t y, const char *text);
void disp_drawStrP(uint8_t x, uint8_t y, PGM_P text);
void disp_drawStrCenteredInBox(uint8_t x, uint8_t y, uint8_t w, uint8_t h, const char *text);
void disp_drawBox(uint8_t x, uint8_t y, uint8_t w, uint8_t h);
void disp_drawFrame(uint8_t x, uint8_t y, uint8_t w, uint8_t h);
void disp_drawHLine(uint8_t x, uint8_t y, uint8_t w);
void disp_drawPixel(uint8_t x, uint8_t y);
void disp_drawDisc(uint8_t x, uint8_t y, uint8_t r);
void disp_drawCircleArc(uint8_t x, uint8_t y, uint8_t r);
void disp_drawXBMP(uint8_t x, uint8_t y, uint8_t w, uint8_t h, const uint8_t *bitmap);
uint8_t disp_getStrWidth(const char *text);
uint8_t disp_getStrWidthP(PGM_P text);
uint8_t disp_getFontAscent();
uint8_t disp_getFontDescent();
void disp_setClipWindow(uint8_t clipX0, uint8_t clipY0, uint8_t clipX1, uint8_t clipY1);
void disp_setMaxClipWindow();

#endif // DISPLAY_BACKEND_H
