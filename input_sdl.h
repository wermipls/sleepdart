#pragma once

#include <SDL2/SDL_scancode.h>

void input_sdl_init();
void input_sdl_update();
uint8_t input_sdl_get_key(uint16_t scancode);
