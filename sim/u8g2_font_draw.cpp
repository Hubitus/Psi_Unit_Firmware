/*
 * u8g2_font_draw.cpp — same u8g2_font_6x10_tr as on SH1106
 */

#include "u8g2_font_draw.h"
#include <cstring>

extern "C" {
#include "u8g2_sim.h"
}

extern const uint8_t u8g2_font_6x10_tr[];

static u8g2_t s_u8g2;
static bool s_fontReady = false;

static void u8g2_font_sim_init_once() {
  if (s_fontReady) {
    return;
  }
  std::memset(&s_u8g2, 0, sizeof(s_u8g2));
  u8g2_SetFont(&s_u8g2, u8g2_font_6x10_tr);
  u8g2_SetFontMode(&s_u8g2, 1);
  u8g2_SetFontPosBaseline(&s_u8g2);
  s_fontReady = true;
}

void u8g2_font_sim_set_draw_color(uint8_t color) {
  u8g2_font_sim_init_once();
  s_u8g2.draw_color = color;
}

void u8g2_font_drawStr(uint8_t x, uint8_t yBaseline, const char *text) {
  if (text == nullptr) {
    return;
  }
  u8g2_font_sim_init_once();
  u8g2_DrawStr(&s_u8g2, x, yBaseline, text);
}

uint8_t u8g2_font_strWidth(const char *text) {
  if (text == nullptr) {
    return 0;
  }
  u8g2_font_sim_init_once();
  return (uint8_t)u8g2_GetStrWidth(&s_u8g2, text);
}
