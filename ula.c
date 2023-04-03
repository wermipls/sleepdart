#include "ula.h"
#include "memory.h"

RGB24_t ula_buffer[BUFFER_WIDTH*BUFFER_HEIGHT];

RGB24_t colors_bright[] = {
    {.r = 0x00, .g = 0x00, .b = 0x00}, // black
    {.r = 0x00, .g = 0x00, .b = 0xFF}, // blue
    {.r = 0xFF, .g = 0x00, .b = 0x00}, // red
    {.r = 0xFF, .g = 0x00, .b = 0xFF}, // magenta
    {.r = 0x00, .g = 0xFF, .b = 0x00}, // green
    {.r = 0x00, .g = 0xFF, .b = 0xFF}, // cyan
    {.r = 0xFF, .g = 0xFF, .b = 0x00}, // yellow
    {.r = 0xFF, .g = 0xFF, .b = 0xFF}, // white
};

RGB24_t colors_basic[] = {
    {.r = 0x00, .g = 0x00, .b = 0x00}, // black
    {.r = 0x00, .g = 0x00, .b = 0xD8}, // blue
    {.r = 0xD8, .g = 0x00, .b = 0x00}, // red
    {.r = 0xD8, .g = 0x00, .b = 0xD8}, // magenta
    {.r = 0x00, .g = 0xD8, .b = 0x00}, // green
    {.r = 0x00, .g = 0xD8, .b = 0xD8}, // cyan
    {.r = 0xD8, .g = 0xD8, .b = 0x00}, // yellow
    {.r = 0xD8, .g = 0xD8, .b = 0xD8}, // white
};

uint8_t border = 0;

void ula_set_border(uint8_t color)
{
    border = color & 7;
}

static inline uint8_t ula_get_screen_byte(uint16_t offset)
{
    return memory_bus_peek(0x4000 + offset);
}

static inline void ula_process_screen_8x1(uint8_t x, uint8_t y, RGB24_t *buf)
{
    uint16_t pix_offset = x;
    pix_offset |= (y & 7) << 8;    // bits 0-2
    pix_offset |= (y & 0x38) << 2; // bits 3-5
    pix_offset |= (y & 0xC0) << 5; // bits 6-7

    uint16_t attrib_offset = 0x1800 + (((y >> 3) << 5) | x);

    uint8_t attrib = ula_get_screen_byte(attrib_offset);
    uint8_t pixel = ula_get_screen_byte(pix_offset);

    uint8_t bright = attrib & (1<<6);
    RGB24_t *colors = bright ? colors_bright : colors_basic;

    RGB24_t ink = colors[attrib & 7];
    RGB24_t paper = colors[(attrib >> 3) & 7];

    buf += 7;
    for (uint8_t i = 0; i < 8; i++) {
        uint8_t p = (pixel >> i) & 1;
        *buf = p ? ink : paper;
        buf--;
    }
}

static inline void ula_fill_border_8x1(RGB24_t *buf)
{
    RGB24_t color = colors_basic[border];

    for (uint8_t i = 0; i < 8; i++) {
        *buf = color;
        buf++;
    }
}

void ula_naive_draw()
{
    // hacky border draw
    RGB24_t *bufptr = ula_buffer;
    for (int i = 0; i < BUFFER_HEIGHT * BUFFER_WIDTH / 8; i++) {
        ula_fill_border_8x1(bufptr);
        bufptr += 8;
    }

    int screen_startx = (BUFFER_WIDTH - 256) / 2;
    int screen_starty = (BUFFER_HEIGHT - 192) / 2;
    int buf_borderwidth = BUFFER_WIDTH - 256;

    bufptr = &ula_buffer[BUFFER_WIDTH * screen_starty + screen_startx];

    for (int y = 0; y < 192; y++) {
        for (int x = 0; x < 32; x++) {
            ula_process_screen_8x1(x, y, bufptr);
            bufptr += 8;
        }
        bufptr += buf_borderwidth;
    }
}
