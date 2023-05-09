#include "keyboard_macro.h"

static const KeyboardMacro_t *current = NULL;
static int frame = 0;
static int index = 0;
static int macro_len = 0;
static uint8_t state[8] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };

void keyboard_macro_play(const KeyboardMacro_t *macro, size_t len)
{
    current = macro;
    frame = 0;
    index = 0;
    macro_len = len;
    for (int i = 0; i < 8; i++) {
        state[i] = 0xFF;
    }
}

void keyboard_macro_process()
{
    if (current == NULL) {
        return;
    }

    for (int i = 0; i < 8; i++) {
        state[i] = 0xFF;
    }

    if (index >= macro_len) {
        current = NULL;
        return;
    }

    frame++;

    while (current[index].frame < frame) {
        index++;
        if (index >= macro_len) {
            return;
        }
    }

    while (current[index].frame == frame) {
        switch (current[index].cmd)
        {
        case KBMACRO_GOTO:
            frame = current[index].value - 1;
            index = 0;
            return;
        case KBMACRO_KEY: ;
            int addr = (current[index].value / 5) & 7;
            int bit = current[index].value % 5;
            state[addr] &= ~(1<<bit);
            break;
        }

        index++;
        if (index >= macro_len) {
            return;
        }
    }
}

uint8_t keyboard_macro_get(int address)
{
    return state[address];
}
