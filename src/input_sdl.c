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
    SDL_Event events[1024];
    int count = SDL_PeepEvents(events, 1024, SDL_PEEKEVENT, SDL_FIRSTEVENT, SDL_LASTEVENT);

    for (int i = 0; i < count; i++) {
        e = events[i];
        switch (e.type)
        {
        case SDL_QUIT:
            quit = 1;
            break;
        case SDL_DROPFILE:
            machine_open_file(e.drop.file);
            SDL_free(e.drop.file);
            break;
        }
    }

    return quit;
}

void input_sdl_pump_events()
{
    SDL_PumpEvents();
}

void input_sdl_flush_events()
{
    SDL_FlushEvents(SDL_FIRSTEVENT, SDL_LASTEVENT);
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
