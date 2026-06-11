/*
 * sim_host.cpp — SDL2 window and framebuffer rendering
 */

#include "display_backend.h"
#include "display_backend_sdl.h"
#include "config.h"
#include "sdl_win.h"

static SDL_Window *s_window = nullptr;
static SDL_Renderer *s_renderer = nullptr;
static SDL_Texture *s_texture = nullptr;
static const int SCALE = 4;

bool sim_host_init() {
  if (SDL_Init(SDL_INIT_VIDEO) != 0) {
    return false;
  }
  SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");
  s_window = SDL_CreateWindow("Psi Unit UI Simulator", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                              DISP_WIDTH * SCALE, DISP_HEIGHT * SCALE, SDL_WINDOW_SHOWN);
  if (s_window == nullptr) {
    return false;
  }
  s_renderer = SDL_CreateRenderer(s_window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
#if SDL_VERSION_ATLEAST(2, 0, 19)
  if (s_renderer != nullptr) {
    SDL_RenderSetIntegerScale(s_renderer, SDL_TRUE);
  }
#endif
  if (s_renderer == nullptr) {
    return false;
  }
  s_texture = SDL_CreateTexture(s_renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, DISP_WIDTH,
                                DISP_HEIGHT);
  return s_texture != nullptr;
}

void sim_host_shutdown() {
  if (s_texture) {
    SDL_DestroyTexture(s_texture);
    s_texture = nullptr;
  }
  if (s_renderer) {
    SDL_DestroyRenderer(s_renderer);
    s_renderer = nullptr;
  }
  if (s_window) {
    SDL_DestroyWindow(s_window);
    s_window = nullptr;
  }
  SDL_Quit();
}

void sim_host_present() {
  if (s_texture == nullptr) {
    return;
  }
  static uint32_t pixels[DISP_WIDTH * DISP_HEIGHT];
  const uint8_t *fb = disp_sim_framebuffer();
  for (int y = 0; y < DISP_HEIGHT; y++) {
    for (int x = 0; x < DISP_WIDTH; x++) {
      const uint16_t idx = (uint16_t)(y / 8) * DISP_WIDTH + x;
      const uint8_t on = (fb[idx] >> (y & 7)) & 1;
      pixels[y * DISP_WIDTH + x] = on ? 0xFFE8E8E8 : 0xFF101010;
    }
  }
  SDL_UpdateTexture(s_texture, nullptr, pixels, DISP_WIDTH * (int)sizeof(uint32_t));
  SDL_RenderClear(s_renderer);
  SDL_RenderCopy(s_renderer, s_texture, nullptr, nullptr);
  SDL_RenderPresent(s_renderer);
}
