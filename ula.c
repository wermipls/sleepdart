#include "ula.h"
#include "memory.h"

struct Write
{
    uint8_t is_write;
    uint8_t value;
    uint64_t cycle;
};

RGB24_t ula_buffer[BUFFER_LEN];

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
uint8_t frame = 0;

#define ULA_WRITES_SIZE 20000
struct Write ula_writes[ULA_WRITES_SIZE];
size_t cur_ula_write_index = 0;

void ula_set_border(uint8_t color, uint64_t cycle)
{
    struct Write w = {.is_write = 1, .cycle = cycle, .value = color & 7};
    ula_writes[cur_ula_write_index] = w;
    if (cur_ula_write_index < ULA_WRITES_SIZE-1) cur_ula_write_index++;
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

    uint8_t flash = attrib & (1<<7);
    RGB24_t ink, paper;
    if (flash && ((frame % 32) > 16)) {
        ink = colors[(attrib >> 3) & 7];
        paper = colors[attrib & 7];
    } else {
        ink = colors[attrib & 7];
        paper = colors[(attrib >> 3) & 7];
    }

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

const uint64_t first_border_cycle = T_FIRSTPIXEL 
                             - T_SCANLINE * (BUFFER_HEIGHT - 192) / 2 
                             - T_EIGHTPX * (BUFFER_WIDTH - 256) / 8 / 2; 

static inline int get_cycle_buf_pos(uint64_t cycle)
{
    if (cycle < first_border_cycle) 
        return -1;

    cycle -= first_border_cycle;

    int x = (cycle % T_SCANLINE) * 2;
    int y = cycle / T_SCANLINE;

    if (x > T_EIGHTPX * BUFFER_WIDTH / 8) 
        x = T_EIGHTPX * BUFFER_WIDTH / 8 - 1;

    int result = y * BUFFER_WIDTH + x;

    if (result < BUFFER_LEN)
        return result;

    return -2;
}

static inline void ula_process_border(RGB24_t *buf)
{
    int last_buf_pos = 0;
    size_t write_i;
    struct Write w = {.is_write = 0};
    for (write_i = 0; write_i < ULA_WRITES_SIZE; write_i++) {
        w = ula_writes[write_i];
        if (!w.is_write) break;
        int pos = get_cycle_buf_pos(w.cycle);
        switch (pos)
        {
        case -1:
            border = w.value;
            break;
        case -2:
            for (int i = (last_buf_pos>>3)<<3; i < BUFFER_LEN; i+=8) {
                ula_fill_border_8x1(&buf[i]);
            }
            border = w.value;
            last_buf_pos = BUFFER_LEN;
            break;
        default:
            for (int i = (last_buf_pos>>3)<<3; i < (pos>>3)<<3; i+=8) {
                ula_fill_border_8x1(&buf[i]);
            }
            last_buf_pos = pos;
            border = w.value;
        }
    }

    for (int i = (last_buf_pos>>3)<<3; i < BUFFER_LEN; i+=8) {
        ula_fill_border_8x1(&buf[i]);
    }

    for (size_t i = 0; i < write_i+1; i++) {
        ula_writes[i].is_write = 0;
    }

    cur_ula_write_index = 0;
}

void ula_naive_draw()
{
    RGB24_t *bufptr = ula_buffer;
    ula_process_border(bufptr);

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

    frame++;
}
