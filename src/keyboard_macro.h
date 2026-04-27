#pragma once

#include <stdint.h>
#include <stddef.h>

typedef struct KeyboardMacro KeyboardMacro_t;

void keyboard_macro_init();
void keyboard_macro_play(const KeyboardMacro_t *macro);
void keyboard_macro_play_tapeload();
void keyboard_macro_process();
uint8_t keyboard_macro_get(int address);
KeyboardMacro_t *keyboard_macro_parse(const char *path);
void keyboard_macro_free(KeyboardMacro_t *macro);
