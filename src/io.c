#include "io.h"
#include "ula.h"
#include "keyboard.h"
#include "machine.h"

uint64_t last_tape_read = 0;
uint64_t last_tape_read_frame = 0;

uint8_t io_handle_contention(uint16_t addr, uint64_t cycle)
{
    // i/o contention on 48/128k is quite funny, as the pattern depends on:
    // - whether high byte is between 0x40 and 0x7F (it "looks" like
    //   contended memory access to the ULA)
    // - low bit being set
    // this works out to a total of four combinations which need to be handled.
    // see https://sinclair.wiki.zxnet.co.uk/wiki/Contended_I/O for details

    uint8_t pattern = ((addr >= 0x4000 && addr < 0x8000) << 1) | (addr & 1);
    uint8_t contention = 0;
    switch (pattern) 
    {
    case 0: // bit reset and no "contended memory"
        cycle += 1;
        contention = ula_get_contention_cycles(cycle);
        break;
    case 1: // bit set and no "contended memory"
        break;
    case 2: // bit reset and "contended memory"
        contention = ula_get_contention_cycles(cycle);
        cycle += 1;
        contention += ula_get_contention_cycles(cycle+contention);
        break;
    case 3: // bit set and "contended memory"
        // LMAO
        contention = ula_get_contention_cycles(cycle);
        cycle += 1;
        contention += ula_get_contention_cycles(cycle+contention);
        cycle += 1;
        contention += ula_get_contention_cycles(cycle+contention);
        cycle += 1;
        contention += ula_get_contention_cycles(cycle+contention);
        break;
    }
    return contention;
}

/* Performs a port write.
 * Returns the amount of extra cycles stalled due to ULA contention. */
uint8_t io_port_write(struct Machine *ctx, uint16_t addr, uint8_t value)
{
    if (!(addr & 1)) {
        ula_set_border(value, ctx->cpu.cycles);
        beeper_write(&ctx->beeper, !(!(value & (1<<4))), ctx->cpu.cycles);
    }

    if (addr == 0xFFFD) {
        ay_write_address(ctx->ay, value);
    }

    if (addr == 0xBFFD) {
        ay_write_data(ctx->ay, value);
    }

    return io_handle_contention(addr, ctx->cpu.cycles);
}

/* Performs a port read. 
 * Returns the amount of extra cycles stalled due to ULA contention. */
uint8_t io_port_read(struct Machine *ctx, uint16_t addr, uint8_t *dest)
{
    if (!(addr & 1)) {
        *dest = keyboard_read(addr) & ~(1<<6);

        if (ctx->player != NULL) {
            uint64_t delta;

            delta = (ctx->frames - last_tape_read_frame) * ctx->timing.t_frame;
            delta += ctx->cpu.cycles;
            delta -= last_tape_read;

            last_tape_read = ctx->cpu.cycles;
            last_tape_read_frame = ctx->frames;

            uint8_t tape = tape_player_get_next_sample(ctx->player, delta);
            if (tape) {
                *dest |= (1<<6);
            }
        }
    } else if (addr == 0xFFFD || addr == 0xBFFD) {
        *dest = ay_read_data(ctx->ay); 
    } else {
        // floating bus.
        // the byte spat out will be either whatever ULA is reading, or 0xFF.
        // more info: https://sinclair.wiki.zxnet.co.uk/wiki/Floating_bus
        *dest = 0xFF;

        if (ctx->cpu.cycles >= ctx->timing.t_firstpx) {
            uint64_t cycle = ctx->cpu.cycles - ctx->timing.t_firstpx;
            int cyc_x = cycle % ctx->timing.t_scanline;
            int y = cycle / ctx->timing.t_scanline;

            if (y < 192 && cyc_x < ctx->timing.t_screen) {
                uint16_t pix_offset = 0x4000
                                    | ((y & 7) << 8)     // bits 0-2
                                    | ((y & 0x38) << 2)  // bits 3-5
                                    | ((y & 0xC0) << 5); // bits 6-7

                uint16_t attrib_offset = 0x5800 | ((y >> 3) << 5);

                int x_offset = (cyc_x / 8) * 2;
                switch (cyc_x & 7) {
                    case 0: *dest = memory_bus_peek(ctx->memory.bus, pix_offset + x_offset); break;
                    case 1: *dest = memory_bus_peek(ctx->memory.bus, attrib_offset + x_offset); break;
                    case 2: *dest = memory_bus_peek(ctx->memory.bus, pix_offset + x_offset + 1); break;
                    case 3: *dest = memory_bus_peek(ctx->memory.bus, attrib_offset+ x_offset + 1); break;
                }
            }
        }
    }

    return io_handle_contention(addr, ctx->cpu.cycles);
}
