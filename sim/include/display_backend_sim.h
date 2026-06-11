#ifndef DISPLAY_BACKEND_SIM_H
#define DISPLAY_BACKEND_SIM_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void disp_sim_write_pixel(uint8_t x, uint8_t y, uint8_t color);

#ifdef __cplusplus
}
#endif

#endif
