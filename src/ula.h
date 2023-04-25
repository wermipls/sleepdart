#pragma once

#include <stdint.h>
#include "palette.h"

#define BUFFER_WIDTH 352
#define BUFFER_HEIGHT 288
#define BUFFER_LEN (BUFFER_WIDTH*BUFFER_HEIGHT)

typedef struct RGB24
{
    uint8_t r;
    uint8_t g;
    uint8_t b;
} RGB24_t;

extern RGB24_t ula_buffer[BUFFER_WIDTH*BUFFER_HEIGHT];

struct Machine;

void ula_init(struct Machine *ctx);
uint8_t ula_get_contention_cycles(uint64_t cycle);
void ula_set_border(uint8_t color, uint64_t cycle);
void ula_write_screen(uint64_t cycle, uint8_t value, uint64_t addr);
void ula_draw_frame();
void ula_set_palette(Palette_t *palette);
