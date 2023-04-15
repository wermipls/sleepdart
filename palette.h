#pragma once

#include <stdint.h>

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
void palette_free(Palette_t *palette);

