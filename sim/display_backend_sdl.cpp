/*
 * display_backend_sdl.cpp — 128×64 framebuffer, U8g2 page loop emulation (8×8 px)
 */

#include "display_backend.h"
#include "u8g2_font_draw.h"
#include <string.h>

static uint8_t s_fb[DISP_WIDTH * DISP_HEIGHT / 8];
static uint8_t s_drawColor = 1;
static bool s_rotated = false;
static uint8_t s_clipX0 = 0;
static uint8_t s_clipY0 = 0;
static uint8_t s_clipX1 = DISP_WIDTH - 1;
static uint8_t s_clipY1 = DISP_HEIGHT - 1;

static bool disp_inClip(int16_t x, int16_t y) {
  return x >= s_clipX0 && x <= s_clipX1 && y >= s_clipY0 && y <= s_clipY1;
}

static void disp_setPixelRaw(uint8_t x, uint8_t y, uint8_t color) {
  if (x >= DISP_WIDTH || y >= DISP_HEIGHT) {
    return;
  }
  const uint16_t idx = (uint16_t)(y / 8) * DISP_WIDTH + x;
  const uint8_t mask = (uint8_t)(1U << (y & 7));
  if (color == 2) {
    s_fb[idx] ^= mask;
  } else if (color) {
    s_fb[idx] |= mask;
  } else {
    s_fb[idx] &= (uint8_t)~mask;
  }
}

static void disp_mapPoint(uint8_t &x, uint8_t &y) {
  if (!s_rotated) {
    return;
  }
  const uint8_t ox = x;
  x = (uint8_t)(DISP_WIDTH - 1 - y);
  y = ox;
}

static void disp_setPixel(uint8_t x, uint8_t y) {
  disp_mapPoint(x, y);
  if (!disp_inClip(x, y)) {
    return;
  }
  disp_setPixelRaw(x, y, s_drawColor);
}

extern "C" {

void disp_sim_write_pixel(uint8_t x, uint8_t y, uint8_t color) {
  const uint8_t prev = s_drawColor;
  s_drawColor = color;
  disp_setPixel(x, y);
  s_drawColor = prev;
}

} /* extern "C" */

static void disp_clearPageBand(uint8_t page) {
  memset(&s_fb[page * DISP_WIDTH], 0, DISP_WIDTH);
}

const uint8_t *disp_sim_framebuffer() {
  return s_fb;
}

void disp_begin(uint8_t) {
  memset(s_fb, 0, sizeof(s_fb));
}

void disp_wake() {}
void disp_setContrast(uint8_t) {}
void disp_setInverted(bool) {}
void disp_setRotation180(bool rotated) {
  s_rotated = rotated;
}

void disp_clear() {
  memset(s_fb, 0, sizeof(s_fb));
}

void disp_setPowerSave(bool) {}

void disp_renderPages(DispDrawCallback drawFn) {
  if (drawFn == nullptr) {
    return;
  }
  for (uint8_t page = 0; page < 8; page++) {
    disp_clearPageBand(page);
    s_clipX0 = 0;
    s_clipY0 = (uint8_t)(page * 8);
    s_clipX1 = DISP_WIDTH - 1;
    s_clipY1 = (uint8_t)(s_clipY0 + 7);
    drawFn();
  }
}

void disp_setFontUi() {
  u8g2_font_sim_set_draw_color(s_drawColor);
}

void disp_setFontToast() {
  u8g2_font_sim_set_draw_color(s_drawColor);
}

void disp_setDrawColor(uint8_t color) {
  s_drawColor = color;
}

void disp_drawStr(uint8_t x, uint8_t y, const char *text) {
  u8g2_font_sim_set_draw_color(s_drawColor);
  u8g2_font_drawStr(x, y, text);
}

void disp_drawStrP(uint8_t x, uint8_t y, PGM_P text) {
  disp_drawStr(x, y, text);
}

void disp_drawStrCenteredInBox(uint8_t x, uint8_t y, uint8_t w, uint8_t h, const char *text) {
  const uint8_t tw = disp_getStrWidth(text);
  const uint8_t ascent = disp_getFontAscent();
  const uint8_t descent = disp_getFontDescent();
  const uint8_t tx = (uint8_t)(x + (w - tw) / 2);
  const uint8_t ty = (uint8_t)(y + (h + ascent - descent) / 2);
  disp_drawStr(tx, ty, text);
}

void disp_drawBox(uint8_t x, uint8_t y, uint8_t w, uint8_t h) {
  if (w == 0 || h == 0) {
    return;
  }
  for (uint8_t i = 0; i < w; i++) {
    disp_setPixel((uint8_t)(x + i), y);
    disp_setPixel((uint8_t)(x + i), (uint8_t)(y + h - 1));
  }
  for (uint8_t j = 0; j < h; j++) {
    disp_setPixel(x, (uint8_t)(y + j));
    disp_setPixel((uint8_t)(x + w - 1), (uint8_t)(y + j));
  }
  /* U8g2 drawBox: color 1 = fill; XOR (2) inverts the entire rectangle (splash logo) */
  if (s_drawColor == 1 || s_drawColor == 2) {
    for (uint8_t j = 1; j + 1 < h; j++) {
      for (uint8_t i = 1; i + 1 < w; i++) {
        disp_setPixel((uint8_t)(x + i), (uint8_t)(y + j));
      }
    }
  }
}

void disp_drawFrame(uint8_t x, uint8_t y, uint8_t w, uint8_t h) {
  disp_drawBox(x, y, w, h);
  if (s_drawColor == 1) {
    return;
  }
  if (s_drawColor == 2) {
    return;
  }
  for (uint8_t j = 1; j + 1 < h; j++) {
    for (uint8_t i = 1; i + 1 < w; i++) {
      disp_setPixel((uint8_t)(x + i), (uint8_t)(y + j));
    }
  }
}

void disp_drawHLine(uint8_t x, uint8_t y, uint8_t w) {
  for (uint8_t i = 0; i < w; i++) {
    disp_setPixel((uint8_t)(x + i), y);
  }
}

void disp_drawPixel(uint8_t x, uint8_t y) {
  disp_setPixel(x, y);
}

void disp_drawDisc(uint8_t x, uint8_t y, uint8_t r) {
  if (r == 0) {
    disp_setPixel(x, y);
    return;
  }
  const int16_t r2 = (int16_t)r * (int16_t)r;
  for (int16_t dy = -r; dy <= r; dy++) {
    for (int16_t dx = -r; dx <= r; dx++) {
      if (dx * dx + dy * dy <= r2) {
        disp_setPixel((uint8_t)(x + dx), (uint8_t)(y + dy));
      }
    }
  }
}

void disp_drawCircleArc(uint8_t x, uint8_t y, uint8_t r) {
  if (r == 0) {
    return;
  }
  const int16_t r2 = (int16_t)r * (int16_t)r;
  const int16_t inner = (int16_t)(r - 1) * (int16_t)(r - 1);
  for (int16_t dy = -r; dy <= 0; dy++) {
    for (int16_t dx = -r; dx <= r; dx++) {
      const int16_t d2 = dx * dx + dy * dy;
      if (d2 <= r2 && d2 >= inner) {
        disp_setPixel((uint8_t)(x + dx), (uint8_t)(y + dy));
      }
    }
  }
}

/* U8g2 drawXBMP: row by row, LSB on the left (u8g2_DrawHXBMP) */
void disp_drawXBMP(uint8_t x, uint8_t y, uint8_t w, uint8_t h, const uint8_t *bitmap) {
  if (bitmap == nullptr || w == 0 || h == 0) {
    return;
  }
  const uint8_t bytesPerRow = (uint8_t)((w + 7) / 8);
  for (uint8_t row = 0; row < h; row++) {
    const uint8_t *rowBits = bitmap + (uint16_t)row * bytesPerRow;
    uint8_t mask = 1;
    for (uint8_t col = 0; col < w; col++) {
      const uint8_t byteIdx = (uint8_t)(col / 8);
      if (rowBits[byteIdx] & mask) {
        disp_setPixel((uint8_t)(x + col), (uint8_t)(y + row));
      }
      mask = (uint8_t)(mask << 1);
      if (mask == 0) {
        mask = 1;
      }
    }
  }
}

uint8_t disp_getStrWidth(const char *text) {
  return u8g2_font_strWidth(text);
}

uint8_t disp_getStrWidthP(PGM_P text) {
  return disp_getStrWidth(text);
}

uint8_t disp_getFontAscent() {
  return 19;
}

uint8_t disp_getFontDescent() {
  return 4;
}

void disp_setClipWindow(uint8_t clipX0, uint8_t clipY0, uint8_t clipX1, uint8_t clipY1) {
  s_clipX0 = clipX0;
  s_clipY0 = clipY0;
  s_clipX1 = clipX1;
  s_clipY1 = clipY1;
}

void disp_setMaxClipWindow() {
  s_clipX0 = 0;
  s_clipY0 = 0;
  s_clipX1 = DISP_WIDTH - 1;
  s_clipY1 = DISP_HEIGHT - 1;
}
