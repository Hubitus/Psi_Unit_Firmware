/*
 * u8g2_sim_glue.c — UTF-8/ASCII and U8g2 lines → simulator framebuffer
 */

#include "u8g2_sim.h"
#include "display_backend_sim.h"

void u8x8_utf8_init(u8x8_t *u8x8) {
  if (u8x8 != NULL) {
    u8x8->utf8_state = 0;
  }
}

uint16_t u8x8_ascii_next(U8X8_UNUSED u8x8_t *u8x8, uint8_t b) {
  if (b == 0 || b == '\n') {
    return 0x0ffff;
  }
  return b;
}

uint16_t u8x8_utf8_next(u8x8_t *u8x8, uint8_t b) {
  if (b == 0 || b == '\n') {
    return 0x0ffff;
  }
  if (u8x8->utf8_state == 0) {
    if (b >= 0xfc) {
      u8x8->utf8_state = 5;
      b &= 1;
    } else if (b >= 0xf8) {
      u8x8->utf8_state = 4;
      b &= 3;
    } else if (b >= 0xf0) {
      u8x8->utf8_state = 3;
      b &= 7;
    } else if (b >= 0xe0) {
      u8x8->utf8_state = 2;
      b &= 15;
    } else if (b >= 0xc0) {
      u8x8->utf8_state = 1;
      b &= 0x01f;
    } else {
      return b;
    }
    u8x8->encoding = b;
    return 0x0fffe;
  }
  u8x8->utf8_state--;
  u8x8->encoding <<= 6;
  b &= 0x03f;
  u8x8->encoding |= b;
  if (u8x8->utf8_state != 0) {
    return 0x0fffe;
  }
  return u8x8->encoding;
}

void u8g2_DrawHLine(u8g2_t *u8g2, u8g2_uint_t x, u8g2_uint_t y, u8g2_uint_t len) {
  if (u8g2 == NULL) {
    return;
  }
  const uint8_t color = u8g2->draw_color;
  for (u8g2_uint_t i = 0; i < len; i++) {
    disp_sim_write_pixel((uint8_t)(x + i), (uint8_t)y, color);
  }
}

void u8g2_DrawHVLine(u8g2_t *u8g2, u8g2_uint_t x, u8g2_uint_t y, u8g2_uint_t len, uint8_t dir) {
  if (dir == 0) {
    u8g2_DrawHLine(u8g2, x, y, len);
  }
}

uint8_t u8g2_GetKerning(u8g2_t *u8g2, u8g2_kerning_t *kerning, uint16_t e1, uint16_t e2) {
  (void)u8g2;
  (void)kerning;
  (void)e1;
  (void)e2;
  return 0;
}

uint8_t u8g2_GetKerningByTable(u8g2_t *u8g2, const uint16_t *kerning_table, uint16_t e1, uint16_t e2) {
  (void)u8g2;
  (void)kerning_table;
  (void)e1;
  (void)e2;
  return 0;
}
