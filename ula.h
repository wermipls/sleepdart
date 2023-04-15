#pragma once

#include <stdint.h>
#include "memory.h"
#include "palette.h"

#define BUFFER_WIDTH 352
#define BUFFER_HEIGHT 288
#define BUFFER_LEN (BUFFER_WIDTH*BUFFER_HEIGHT)
#define T_FIRSTPIXEL 14336
#define T_SCANLINE 224
#define T_SCREEN 128
#define T_FRAME (T_SCANLINE*312)
#define T_EIGHTPX (T_SCREEN / 32)

typedef struct RGB24
{
    uint8_t r;
    uint8_t g;
    uint8_t b;
} RGB24_t;

extern RGB24_t ula_buffer[BUFFER_WIDTH*BUFFER_HEIGHT];

uint8_t ula_get_contention_cycles(uint64_t cycle);
void ula_set_border(uint8_t color, uint64_t cycle);
void ula_naive_draw(Memory_t *mem);
void ula_set_palette(Palette_t *palette);
