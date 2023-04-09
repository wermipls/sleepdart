#include "memory.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "log.h"
#include "ula.h"

uint8_t memory_bus[0x10000] = {0};

/* Initializes the DRAM to a pseudo-random state it would have on initial power-on. */
void memory_init()
{
    uint16_t ram_start = 0x4000;
    uint16_t ram_size = 0x10000 - ram_start;

    uint8_t *p = &memory_bus[ram_start];

    srand(time(NULL));

    for (uint16_t i = 0; i < ram_size; i++) {
        *p = rand();
        p++;
    }
}

/* Loads a 16K ROM into the beginning of memory space. 
 * Returns zero on success, non-zero otherwise. */
int memory_load_rom_16k(char path[])
{
    FILE *f = fopen(path, "rb");
    if (!f) {
        dlog(LOG_ERR, "Failed to load ROM \"%s\"", path);
        return -1;
    }
    size_t bytes = fread(memory_bus, 1, 0x4000, f);
    fclose(f);
    dlog(LOG_INFO, "Loaded %d bytes from \"%s\"", bytes, path);
    return 0;
}

/* Performs a memory bus write. 
 * Returns the amount of extra cycles stalled due to ULA memory contention. */
uint8_t memory_write(uint16_t addr, uint8_t value, uint64_t cycle)
{
    if (addr < 0x4000) { 
        // 0x0000 - 0x3FFF -> ROM 
        // no-op for now
    } else if (addr < 0x8000) {
        // 0x4000 - 0x7FFF -> RAM (contended memory)
        memory_bus[addr] = value; // ostrożnie!
        return ula_get_contention_cycles(cycle);
    } else {
        // 0x8000 - 0xFFFF -> RAM
        memory_bus[addr] = value;
    }

    return 0;
}

/* Performs a memory bus read. 
 * Returns the amount of extra cycles stalled due to ULA memory contention. */
uint8_t memory_read(uint16_t addr, uint8_t *dest, uint64_t cycle)
{
    *dest = memory_bus[addr];
    if (addr >= 0x4000 && addr < 0x8000) {
        return ula_get_contention_cycles(cycle);
    }
    return 0; 
}

/* Returns a byte on the memory bus from a given address.
 * Mostly meant for debug purposes. For hardware interaction,
 * use memory_read(). */
uint8_t memory_bus_peek(uint16_t addr)
{
    return memory_bus[addr];
}
