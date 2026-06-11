/*
 * u8g2_sim.h — minimal U8g2 types for the simulator (font and line drawing only)
 */
#ifndef U8G2_SIM_H
#define U8G2_SIM_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef U8X8_UNUSED
#define U8X8_UNUSED
#endif
#ifndef U8G2_NOINLINE
#define U8G2_NOINLINE
#endif
#ifndef U8G2_FONT_SECTION
#define U8G2_FONT_SECTION(name)
#endif

#define U8G2_FONT_HEIGHT_MODE_TEXT 0
#define U8G2_FONT_HEIGHT_MODE_XTEXT 1
#define U8G2_FONT_HEIGHT_MODE_ALL 2

typedef uint8_t u8g2_uint_t;
typedef int8_t u8g2_int_t;

typedef struct u8x8_struct u8x8_t;
typedef struct u8g2_struct u8g2_t;

typedef uint16_t (*u8x8_char_cb)(u8x8_t *u8x8, uint8_t b);
typedef u8g2_uint_t (*u8g2_font_calc_vref_fnptr)(u8g2_t *u8g2);
typedef void (*u8g2_draw_ll_hvline_cb)(u8g2_t *u8g2, u8g2_uint_t x, u8g2_uint_t y, u8g2_uint_t len, uint8_t dir);

struct _u8g2_font_info_t {
  uint8_t glyph_cnt;
  uint8_t bbx_mode;
  uint8_t bits_per_0;
  uint8_t bits_per_1;
  uint8_t bits_per_char_width;
  uint8_t bits_per_char_height;
  uint8_t bits_per_char_x;
  uint8_t bits_per_char_y;
  uint8_t bits_per_delta_x;
  int8_t max_char_width;
  int8_t max_char_height;
  int8_t x_offset;
  int8_t y_offset;
  int8_t ascent_A;
  int8_t descent_g;
  int8_t ascent_para;
  int8_t descent_para;
  uint16_t start_pos_upper_A;
  uint16_t start_pos_lower_a;
};
typedef struct _u8g2_font_info_t u8g2_font_info_t;

struct _u8g2_font_decode_t {
  const uint8_t *decode_ptr;
  u8g2_uint_t target_x;
  u8g2_uint_t target_y;
  int8_t x;
  int8_t y;
  int8_t glyph_width;
  int8_t glyph_height;
  uint8_t decode_bit_pos;
  uint8_t is_transparent;
  uint8_t fg_color;
  uint8_t bg_color;
};
typedef struct _u8g2_font_decode_t u8g2_font_decode_t;

struct u8x8_struct {
  const void *display_info;
  u8x8_char_cb next_cb;
  void *display_cb;
  void *cad_cb;
  void *byte_cb;
  void *gpio_and_delay_cb;
  uint32_t bus_clock;
  const uint8_t *font;
  uint16_t encoding;
  uint8_t x_offset;
  uint8_t is_font_inverse_mode;
  uint8_t i2c_address;
  uint8_t i2c_started;
  uint8_t utf8_state;
  uint8_t gpio_result;
  uint8_t debounce_default_pin_state;
  uint8_t debounce_last_pin_state;
  uint8_t debounce_state;
  uint8_t debounce_result_msg;
};

struct u8g2_struct {
  u8x8_t u8x8;
  u8g2_draw_ll_hvline_cb ll_hvline;
  const void *cb;
  uint8_t *tile_buf_ptr;
  uint8_t tile_buf_height;
  uint8_t tile_curr_row;
  u8g2_uint_t pixel_buf_width;
  u8g2_uint_t pixel_buf_height;
  u8g2_uint_t pixel_curr_row;
  u8g2_uint_t buf_y0;
  u8g2_uint_t buf_y1;
  u8g2_uint_t width;
  u8g2_uint_t height;
  u8g2_uint_t user_x0;
  u8g2_uint_t user_x1;
  u8g2_uint_t user_y0;
  u8g2_uint_t user_y1;
  const uint8_t *font;
  u8g2_font_calc_vref_fnptr font_calc_vref;
  u8g2_font_decode_t font_decode;
  u8g2_font_info_t font_info;
  uint8_t font_height_mode;
  int8_t font_ref_ascent;
  int8_t font_ref_descent;
  int8_t glyph_x_offset;
  uint8_t bitmap_transparency;
  uint8_t draw_color;
  uint8_t is_auto_page_clear;
};

#define u8g2_GetU8x8(u8g2) ((u8x8_t *)(u8g2))
#define u8x8_pgm_read(addr) (*(const uint8_t *)(addr))

typedef struct u8g2_kerning_struct u8g2_kerning_t;

void u8x8_utf8_init(u8x8_t *u8x8);
uint16_t u8x8_ascii_next(u8x8_t *u8x8, uint8_t b);
uint16_t u8x8_utf8_next(u8x8_t *u8x8, uint8_t b);

void u8g2_DrawHVLine(u8g2_t *u8g2, u8g2_uint_t x, u8g2_uint_t y, u8g2_uint_t len, uint8_t dir);
void u8g2_DrawHLine(u8g2_t *u8g2, u8g2_uint_t x, u8g2_uint_t y, u8g2_uint_t len);

void u8g2_read_font_info(u8g2_font_info_t *font_info, const uint8_t *font);
void u8g2_UpdateRefHeight(u8g2_t *u8g2);
void u8g2_SetFont(u8g2_t *u8g2, const uint8_t *font);
void u8g2_SetFontMode(u8g2_t *u8g2, uint8_t is_transparent);
void u8g2_SetFontPosBaseline(u8g2_t *u8g2);

u8g2_uint_t u8g2_DrawStr(u8g2_t *u8g2, u8g2_uint_t x, u8g2_uint_t y, const char *str);
u8g2_uint_t u8g2_GetStrWidth(u8g2_t *u8g2, const char *s);

uint8_t u8g2_GetKerning(u8g2_t *u8g2, u8g2_kerning_t *kerning, uint16_t e1, uint16_t e2);
uint8_t u8g2_GetKerningByTable(u8g2_t *u8g2, const uint16_t *kerning_table, uint16_t e1, uint16_t e2);

#ifdef __cplusplus
}
#endif

#endif /* U8G2_SIM_H */
