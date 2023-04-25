#pragma once

#include <stdint.h>
#include <stdbool.h>

struct PaletteColor
{
    uint8_t r;
    uint8_t g;
    uint8_t b;
};

typedef struct Palette
{
    size_t colors;
    struct PaletteColor *color;
} Palette_t;

Palette_t *palette_load(const char *path);
Palette_t *palette_load_current();
void palette_free(Palette_t *palette);
void palette_list_init();
void palette_list_free();
char **palette_list_get();
void palette_set_by_index(size_t index);
void palette_set_default();
size_t palette_get_index();
bool palette_has_changed();
