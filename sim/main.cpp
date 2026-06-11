/*
 * main.cpp — desktop UI simulator
 */

#include "app_setup.h"
#include "app_loop.h"
#include "input_backend_sim.h"
#include "sim_host.h"
#include "sdl_win.h"
#include <stdio.h>

static void sim_postWake() {
  sim_input_post(INPUT_WAKE, 0);
}

static bool sim_handle_sdl_event(const SDL_Event &ev) {
  switch (ev.type) {
    case SDL_QUIT:
      return true;
    case SDL_KEYDOWN:
      sim_postWake();
      switch (ev.key.keysym.sym) {
        case SDLK_UP:
          sim_input_post(INPUT_ENCODER, -1);
          break;
        case SDLK_DOWN:
          sim_input_post(INPUT_ENCODER, 1);
          break;
        case SDLK_RETURN:
        case SDLK_KP_ENTER:
          sim_input_post(INPUT_SHORT_PRESS, 0);
          break;
        case SDLK_ESCAPE:
          sim_input_post(INPUT_LONG_PRESS, 0);
          break;
        case SDLK_q:
          return true;
        default:
          break;
      }
      break;
    default:
      break;
  }
  return false;
}

int main(int argc, char **argv) {
  (void)argc;
  (void)argv;

  if (!sim_host_init()) {
    fprintf(stderr, "SDL init failed: %s\n", SDL_GetError());
    return 1;
  }

  app_setup();

  bool quit = false;
  while (!quit) {
    SDL_Event ev;
    while (SDL_PollEvent(&ev)) {
      if (sim_handle_sdl_event(ev)) {
        quit = true;
      }
    }
    app_loop();
    sim_host_present();
    SDL_Delay(5);
  }

  sim_host_shutdown();
  return 0;
}
