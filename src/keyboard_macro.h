#pragma once

#include <inttypes.h>

enum KeyMacroCmd
{
    KBMACRO_KEY,
    KBMACRO_GOTO,
};

typedef struct KeyboardMacro
{
    int frame;
    enum KeyMacroCmd cmd;
    int value;
} KeyboardMacro_t;

void keyboard_macro_play(const KeyboardMacro_t *macro, size_t len);
void keyboard_macro_process();
uint8_t keyboard_macro_get(int address);
