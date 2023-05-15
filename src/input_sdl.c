#include "input_sdl.h"
#include <SDL2/SDL.h>
#include <stdlib.h>
#include <string.h>
#include "machine.h"

static uint8_t *keyboard_state_old = NULL;
static const uint8_t *keyboard_state;
static int keyboard_state_size;

void input_sdl_init()
{
    keyboard_state = SDL_GetKeyboardState(&keyboard_state_size);
    if (keyboard_state_old == NULL) {
        keyboard_state_old = malloc(keyboard_state_size);
    }

    input_sdl_update();
    input_sdl_copy_old_state();
}

void input_sdl_deinit()
{
    if (keyboard_state_old) {
        free(keyboard_state_old);
        keyboard_state_old = NULL;
    }
}

int input_sdl_update()
{
    int quit = 0;

    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        int handled = 0;
        switch (e.type)
        {
        case SDL_QUIT:
            handled = 1;
            quit = 1;
            break;
        case SDL_DROPFILE:
            handled = 1;
            machine_open_file(e.drop.file);
            SDL_free(e.drop.file);
            break;
        }

        if (!handled) {
            SDL_PushEvent(&e);
        }
    }

    return quit;
}

void input_sdl_copy_old_state()
{
    if (keyboard_state_old != NULL) {
        memcpy(keyboard_state_old, keyboard_state, keyboard_state_size);
    }
}

uint8_t input_sdl_get_key(uint16_t scancode)
{
    if (scancode > keyboard_state_size) return 0;
    return keyboard_state[scancode];
}

uint8_t input_sdl_get_key_pressed(uint16_t scancode)
{
    if (scancode > keyboard_state_size) return 0;
    if (keyboard_state_old == NULL) return 0;

    return (keyboard_state_old[scancode] == 0) && keyboard_state[scancode];
}
