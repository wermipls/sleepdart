#include "io.h"
#include "ula.h"
#include "keyboard.h"

/* Performs a port write.
 * Returns the amount of extra cycles stalled due to ULA contention. */
uint8_t io_port_write(uint16_t addr, uint8_t value, uint64_t cycle)
{
    if (!(addr & 1)) {
        ula_set_border(value, cycle);
    }
    return 0;
}

/* Performs a port read. 
 * Returns the amount of extra cycles stalled due to ULA contention. */
uint8_t io_port_read(uint16_t addr, uint8_t *dest, uint64_t cycle)
{
    uint8_t l = addr & 255;
    if (l == 0xFE) {
        *dest = keyboard_read(addr);
    }
    return 0;
}
