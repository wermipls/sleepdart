#include "ula.h"
#include "machine.h"

struct WriteBorder
{
    int cycle;
    int value;
};

struct WriteScreen
{
    int cycle;
    int value;
    int address;
};

RGB24_t ula_buffer[BUFFER_LEN];

RGB24_t colors[] = {
    {.r = 0x00, .g = 0x00, .b = 0x00}, // black
    {.r = 0x00, .g = 0x00, .b = 0xD8}, // blue
    {.r = 0xD8, .g = 0x00, .b = 0x00}, // red
    {.r = 0xD8, .g = 0x00, .b = 0xD8}, // magenta
    {.r = 0x00, .g = 0xD8, .b = 0x00}, // green
    {.r = 0x00, .g = 0xD8, .b = 0xD8}, // cyan
    {.r = 0xD8, .g = 0xD8, .b = 0x00}, // yellow
    {.r = 0xD8, .g = 0xD8, .b = 0xD8}, // white
    // bright
    {.r = 0x00, .g = 0x00, .b = 0x00}, // black
    {.r = 0x00, .g = 0x00, .b = 0xFF}, // blue
    {.r = 0xFF, .g = 0x00, .b = 0x00}, // red
    {.r = 0xFF, .g = 0x00, .b = 0xFF}, // magenta
    {.r = 0x00, .g = 0xFF, .b = 0x00}, // green
    {.r = 0x00, .g = 0xFF, .b = 0xFF}, // cyan
    {.r = 0xFF, .g = 0xFF, .b = 0x00}, // yellow
    {.r = 0xFF, .g = 0xFF, .b = 0xFF}, // white
};

uint8_t border = 0;
uint8_t frame = 0;

#define ULA_WRITES_SIZE 20000
struct WriteBorder writes_border[ULA_WRITES_SIZE];
size_t border_write_index = 0;
struct WriteScreen writes_screen[ULA_WRITES_SIZE];
size_t screen_write_index = 0;

uint8_t screen_dirty[0x1B00];

uint8_t contention_pattern[] = {6, 5, 4, 3, 2, 1, 0, 0};

struct MachineTiming timing;
Memory_t *mem;
uint64_t first_border_cycle;

void ula_reset_screen_dirty()
{
    for (size_t i = 0; i < ULA_WRITES_SIZE; i++) {
        writes_screen[i].cycle = -1;
    }

    screen_write_index = 0;

    for (size_t i = 0; i < sizeof(screen_dirty); i++) {
        screen_dirty[i] = mem->bus[0x4000 + i];
    }
}

void ula_init(struct Machine *ctx)
{
    timing = ctx->timing;
    mem = &ctx->memory;

    first_border_cycle = timing.t_firstpx 
                       - timing.t_scanline * (BUFFER_HEIGHT - 192) / 2 
                       - timing.t_eightpx * (BUFFER_WIDTH - 256) / 8 / 2; 

    for (size_t i = 0; i < ULA_WRITES_SIZE; i++) {
        writes_border[i] = (struct WriteBorder){ .cycle = -1 };
    }

    for (size_t i = 0; i < BUFFER_LEN; i++) {
        ula_buffer[i] = (RGB24_t){ .r = 0, .g = 0, .b = 0 };
    }

    ula_reset_screen_dirty();
}

void ula_set_palette(Palette_t *palette)
{
    if (palette->colors != 16) {
        return;
    }

    for (size_t i = 0; i < 16; i++) {
        colors[i].r = palette->color[i].r;
        colors[i].g = palette->color[i].g;
        colors[i].b = palette->color[i].b;
    }
}

uint8_t ula_get_contention_cycles(uint64_t cycle)
{
    if (cycle < timing.t_firstpx) return 0;
    cycle -= timing.t_firstpx;

    uint16_t line = cycle / timing.t_scanline;
    if (line > 192) return 0;

    uint16_t linecyc = cycle % timing.t_scanline;
    if (linecyc >= timing.t_screen) return 0;

    return contention_pattern[linecyc % 8];
}

void ula_set_border(uint8_t color, uint64_t cycle)
{
    struct WriteBorder w = {.cycle = cycle, .value = color & 7};
    writes_border[border_write_index] = w;
    if (border_write_index < ULA_WRITES_SIZE-2) border_write_index++;
}

void ula_write_screen(uint64_t cycle, uint8_t value, uint64_t addr)
{
    struct WriteScreen w = {.cycle = cycle, .value = value, .address = addr-0x4000};
    writes_screen[screen_write_index] = w;
    if (screen_write_index < ULA_WRITES_SIZE-2) screen_write_index++;
}

static inline uint8_t ula_get_screen_byte(uint16_t offset)
{
    return screen_dirty[offset];
}

static inline void ula_process_screen_8x1(uint8_t x, uint8_t y, RGB24_t *buf)
{
    int cycle = timing.t_firstpx + y * timing.t_scanline + x * timing.t_eightpx;
    struct WriteScreen w = writes_screen[screen_write_index];

    while (w.cycle >= 0) {
        if (cycle < w.cycle) {
            break;
        }
        screen_dirty[w.address] = w.value;
        screen_write_index++;
        w = writes_screen[screen_write_index];
    }

    uint16_t pix_offset = x;
    pix_offset |= (y & 7) << 8;    // bits 0-2
    pix_offset |= (y & 0x38) << 2; // bits 3-5
    pix_offset |= (y & 0xC0) << 5; // bits 6-7

    uint16_t attrib_offset = 0x1800 + (((y >> 3) << 5) | x);

    uint8_t attrib = ula_get_screen_byte(attrib_offset);
    uint8_t pixel = ula_get_screen_byte(pix_offset);

    int bright = (attrib>>6) & 1;

    int flash = attrib & (1<<7);
    RGB24_t ink, paper;
    if (flash && ((frame % 32) > 16)) {
        ink = colors[bright*8 + ((attrib >> 3) & 7)];
        paper = colors[bright*8 + (attrib & 7)];
    } else {
        ink = colors[bright*8 + (attrib & 7)];
        paper = colors[bright*8 + ((attrib >> 3) & 7)];
    }

    buf += 7;
    for (int i = 0; i < 8; i++) {
        uint8_t p = (pixel >> i) & 1;
        *buf = p ? ink : paper;
        buf--;
    }
}

static inline void ula_fill_border_8x1(RGB24_t *buf)
{
    RGB24_t color = colors[border];

    for (uint8_t i = 0; i < 8; i++) {
        *buf = color;
        buf++;
    }
}

static inline int get_cycle_buf_pos(uint64_t cycle)
{
    if (cycle < first_border_cycle) 
        return -1;

    cycle -= first_border_cycle;

    int x = (cycle % timing.t_scanline) * 2;
    int y = cycle / timing.t_scanline;

    if (x > BUFFER_WIDTH) 
        x = BUFFER_WIDTH;

    int result = y * BUFFER_WIDTH + x;

    if (result < BUFFER_LEN)
        return result;

    return -2;
}

static inline void ula_process_border(RGB24_t *buf)
{
    int last_buf_pos = 0;
    size_t write_i;
    struct WriteBorder w = {.cycle = -1};
    for (write_i = 0; write_i < ULA_WRITES_SIZE-1; write_i++) {
        w = writes_border[write_i];
        if (w.cycle < 0) break;
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
        writes_border[i].cycle = -1;
    }

    border_write_index = 0;
}

void ula_draw_frame()
{
    RGB24_t *bufptr = ula_buffer;
    ula_process_border(bufptr);

    int screen_startx = (BUFFER_WIDTH - 256) / 2;
    int screen_starty = (BUFFER_HEIGHT - 192) / 2;
    int buf_borderwidth = BUFFER_WIDTH - 256;

    bufptr = &ula_buffer[BUFFER_WIDTH * screen_starty + screen_startx];
    screen_write_index = 0;

    for (int y = 0; y < 192; y++) {
        for (int x = 0; x < 32; x++) {
            ula_process_screen_8x1(x, y, bufptr);
            bufptr += 8;
        }
        bufptr += buf_borderwidth;
    }

    ula_reset_screen_dirty();

    frame++;
}
