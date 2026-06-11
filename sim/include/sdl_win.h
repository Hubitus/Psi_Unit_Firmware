/*
 * sdl_win.h — SDL2 without Windows macro conflicts (min/max, etc.)
 */
#ifndef SDL_WIN_H
#define SDL_WIN_H

/* Custom main() in main.cpp — do not rename to SDL_main (otherwise LNK2019 unresolved main). */
#ifndef SDL_MAIN_HANDLED
#define SDL_MAIN_HANDLED
#endif

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <SDL.h>

#endif
