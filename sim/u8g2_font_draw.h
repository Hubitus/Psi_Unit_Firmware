#ifndef U8G2_FONT_DRAW_H
#define U8G2_FONT_DRAW_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

extern const uint8_t u8g2_font_6x10_tr[];

void u8g2_font_sim_set_draw_color(uint8_t color);
void u8g2_font_drawStr(uint8_t x, uint8_t yBaseline, const char *text);
uint8_t u8g2_font_strWidth(const char *text);

#ifdef __cplusplus
}
#endif

#endif
