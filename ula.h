#pragma once

#include <stdint.h>

#define BUFFER_WIDTH 352
#define BUFFER_HEIGHT 288
#define T_FIRSTPIXEL 14366
#define T_SCANLINE 224
#define T_SCREEN 128
#define T_FRAME (T_SCANLINE*312)

typedef struct RGB24
{
    uint8_t r;
    uint8_t g;
    uint8_t b;
} RGB24_t;

extern RGB24_t ula_buffer[BUFFER_WIDTH*BUFFER_HEIGHT];

void ula_naive_draw();
