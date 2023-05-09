#include "keyboard.h"
#include "keyboard_macro.h"
#include "input_sdl.h"

// speccy -> sdl scancode key map
const int keyboard_scancode_map[8][5] = {
    { // addr bit 0
        SDL_SCANCODE_LSHIFT,
        SDL_SCANCODE_Z,
        SDL_SCANCODE_X,
        SDL_SCANCODE_C,
        SDL_SCANCODE_V,
    },
    { // addr bit 1
        SDL_SCANCODE_A,
        SDL_SCANCODE_S,
        SDL_SCANCODE_D,
        SDL_SCANCODE_F,
        SDL_SCANCODE_G,
    },
    { // addr bit 2
        SDL_SCANCODE_Q,
        SDL_SCANCODE_W,
        SDL_SCANCODE_E,
        SDL_SCANCODE_R,
        SDL_SCANCODE_T,
    },
    { // addr bit 3
        SDL_SCANCODE_1,
        SDL_SCANCODE_2,
        SDL_SCANCODE_3,
        SDL_SCANCODE_4,
        SDL_SCANCODE_5,
    },
    { // addr bit 4
        SDL_SCANCODE_0,
        SDL_SCANCODE_9,
        SDL_SCANCODE_8,
        SDL_SCANCODE_7,
        SDL_SCANCODE_6,
    },
    { // addr bit 5
        SDL_SCANCODE_P,
        SDL_SCANCODE_O,
        SDL_SCANCODE_I,
        SDL_SCANCODE_U,
        SDL_SCANCODE_Y,
    },
    { // addr bit 6
        SDL_SCANCODE_RETURN, // ENTER
        SDL_SCANCODE_L,
        SDL_SCANCODE_K,
        SDL_SCANCODE_J,
        SDL_SCANCODE_H,
    },
    { // addr bit 7
        SDL_SCANCODE_SPACE,
        SDL_SCANCODE_LCTRL, // SYM SHFT
        SDL_SCANCODE_M,
        SDL_SCANCODE_N,
        SDL_SCANCODE_B,
    },
};

uint8_t keyboard_read(uint16_t addr)
{
    uint8_t h = ~(addr >> 8);
    uint8_t result = 0xFF;

    for (uint8_t a_bit = 0; a_bit < 8; a_bit++) {
        uint8_t mask = (1<<a_bit);

        if (h & mask) {
            for (uint8_t k_bit = 0; k_bit < 5; k_bit++) {
                mask = ~(1<<k_bit);
                int scancode = keyboard_scancode_map[a_bit][k_bit];
                if (input_sdl_get_key(scancode))
                    result &= mask;
            }

            result &= keyboard_macro_get(a_bit);
        }
    }

    return result;
}

