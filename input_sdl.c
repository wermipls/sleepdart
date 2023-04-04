#include "input_sdl.h"
#include <SDL2/SDL.h>

const uint8_t *keyboard_state;
int keyboard_state_size;

void input_sdl_init()
{
    keyboard_state = SDL_GetKeyboardState(&keyboard_state_size);
    input_sdl_update();
}

void input_sdl_update()
{
    SDL_PumpEvents();
}

uint8_t input_sdl_get_key(uint16_t scancode)
{
    if (scancode > keyboard_state_size) return 0;
    return keyboard_state[scancode];
}
