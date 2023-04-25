#pragma once

#include <SDL2/SDL_scancode.h>

void input_sdl_init();
void input_sdl_deinit();
void input_sdl_update();
void input_sdl_copy_old_state();
uint8_t input_sdl_get_key(uint16_t scancode);
uint8_t input_sdl_get_key_pressed(uint16_t scancode);
