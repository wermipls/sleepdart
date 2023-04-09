#include "io.h"
#include "ula.h"
#include "keyboard.h"

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
uint8_t io_port_write(uint16_t addr, uint8_t value, uint64_t cycle)
{
    if (!(addr & 1)) {
        ula_set_border(value, cycle);
    }

    return io_handle_contention(addr, cycle);
}

/* Performs a port read. 
 * Returns the amount of extra cycles stalled due to ULA contention. */
uint8_t io_port_read(uint16_t addr, uint8_t *dest, uint64_t cycle)
{
    uint8_t l = addr & 255;
    if (l == 0xFE) {
        *dest = keyboard_read(addr);
    }

    return io_handle_contention(addr, cycle);
}
