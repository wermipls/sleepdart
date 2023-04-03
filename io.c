#include "io.h"
#include "ula.h"

/* Performs a port write.
 * Returns the amount of extra cycles stalled due to ULA contention. */
uint8_t io_port_write(uint16_t addr, uint8_t value)
{
    if (!(addr & 1)) {
        ula_set_border(value);
    }
    return 0;
}

/* Performs a port read. 
 * Returns the amount of extra cycles stalled due to ULA contention. */
uint8_t io_port_read(uint16_t addr, uint8_t *dest)
{
    return 0; 
}
