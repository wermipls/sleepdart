#include "memory.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "log.h"
#include "ula.h"
#include "machine.h"

/* Initializes the DRAM to a pseudo-random state it would have on initial power-on. */
void memory_init(Memory_t *mem)
{
    uint16_t ram_start = 0x4000;
    uint16_t ram_size = 0x10000 - ram_start;

    uint8_t *p = &mem->bus[ram_start];

    srand(time(NULL));

    for (uint16_t i = 0; i < ram_size; i++) {
        *p = rand();
        p++;
    }
}

/* Loads a 16K ROM into the beginning of memory space. 
 * Returns zero on success, non-zero otherwise. */
int memory_load_rom_16k(Memory_t *mem, char path[])
{
    FILE *f = fopen(path, "rb");
    if (!f) {
        dlog(LOG_ERR, "Failed to load ROM \"%s\"", path);
        return -1;
    }
    size_t bytes = fread(mem->bus, 1, 0x4000, f);
    fclose(f);
    dlog(LOG_INFO, "Loaded %d bytes from \"%s\"", bytes, path);
    return 0;
}

/* Performs a memory bus write. 
 * Returns the amount of extra cycles stalled due to ULA memory contention. */
uint8_t memory_write(struct Machine *ctx, uint16_t addr, uint8_t value)
{
    if (addr < 0x4000) { 
        // 0x0000 - 0x3FFF -> ROM 
        // no-op for now
    } else if (addr < 0x5B00) {
        ctx->memory.bus[addr] = value;
        int contention = ula_get_contention_cycles(ctx->cpu.cycles);
        ula_write_screen(ctx->cpu.cycles + contention, value, addr);
        return contention;
    } else if (addr < 0x8000) {
        // 0x4000 - 0x7FFF -> RAM (contended memory)
        ctx->memory.bus[addr] = value; // ostrożnie!
        return ula_get_contention_cycles(ctx->cpu.cycles);
    } else {
        // 0x8000 - 0xFFFF -> RAM
        ctx->memory.bus[addr] = value;
    }

    return 0;
}

/* Performs a memory bus read. 
 * Returns the amount of extra cycles stalled due to ULA memory contention. */
uint8_t memory_read(struct Machine *ctx, uint16_t addr, uint8_t *dest)
{
    *dest = ctx->memory.bus[addr];
    if (addr >= 0x4000 && addr < 0x8000) {
        return ula_get_contention_cycles(ctx->cpu.cycles);
    }
    return 0; 
}

/* Returns a byte on the memory bus from a given address.
 * Mostly meant for debug purposes. For hardware interaction,
 * use memory_read(). */
uint8_t memory_bus_peek(uint8_t *bus, uint16_t addr)
{
    return bus[addr];
}
