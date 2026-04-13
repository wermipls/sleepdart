#include "memory.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "log.h"
#include "ula.h"
#include "machine.h"
#include <physfs.h>

/* Initializes the DRAM to a pseudo-random state it would have on initial power-on.
 * Also sets the banks to bootup defaults. */
void memory_init(Memory_t *mem)
{
    mem->bank_0000 = mem->rom;
    mem->bank_4000 = &mem->ram[0x4000 * 5];
    mem->bank_8000 = &mem->ram[0x4000 * 2];
    mem->bank_c000 = &mem->ram[0x4000 * 0];
    uint8_t *p = mem->ram;

    srand(time(NULL));

    for (size_t i = 0; i < sizeof(mem->ram); i++) {
        *p = rand();
        p++;
    }
}

/* Loads a ROM into the beginning of memory space.
 * Returns zero on success, non-zero otherwise. */
int memory_load_rom(Memory_t *mem, char path[])
{
    PHYSFS_File *f = PHYSFS_openRead(path);
    if (!f) {
        dlog(LOG_ERR, "Failed to load ROM \"%s\"", path);
        return -1;
    }
    size_t bytes = PHYSFS_readBytes(f, mem->rom, 0x8000);
    PHYSFS_close(f);
    dlog(LOG_INFO, "Loaded %d bytes from \"%s\"", bytes, path);
    return 0;
}

/* Performs a memory bus write.
 * Returns the amount of extra cycles stalled due to ULA memory contention. */
uint8_t memory_write(struct Machine *ctx, uint16_t addr, uint8_t value)
{
    uint8_t addr_hi = addr >> 14;
    uint16_t addr_lo = addr & 0x3fff;
    uint8_t *page = ctx->memory.bank[addr_hi];
    int page_idx = (int)(page - ctx->memory.ram) / 0x4000; // ugly.

    // assume ROM is located before RAM.
    if (page < ctx->memory.ram) {
        return 0; // no-op.
    } 

    page[addr_lo] = value;
    int contention = (page_idx >= 0 && page_idx & 1) ? ula_get_contention_cycles(ctx->cpu.cycles) : 0;
    if (page_idx >= 0 && ((page_idx & 13) == 5) && (addr_lo < 0x1B00)) {
        ula_write_screen(ctx->cpu.cycles + contention, value, addr_lo, page_idx & 2);
    }
    return contention;
}

/* Performs a memory bus read.
 * Returns the amount of extra cycles stalled due to ULA memory contention. */
uint8_t memory_read(struct Machine *ctx, uint16_t addr, uint8_t *dest)
{
    uint8_t addr_hi = addr >> 14;
    uint16_t addr_lo = addr &= 0x3fff;
    uint8_t *page = ctx->memory.bank[addr_hi];
    int page_idx = (int)(page - ctx->memory.ram) / 0x4000; // ugly.

    if (page) {
        *dest = page[addr_lo];
    }
    return (page_idx >= 0 && page_idx & 1) ? ula_get_contention_cycles(ctx->cpu.cycles) : 0;
}

/* Returns a byte on the memory bus from a given address.
 * Mostly meant for debug purposes. For hardware interaction,
 * use memory_read(). */
uint8_t memory_bus_peek(Memory_t *mem, uint16_t addr)
{
    uint8_t addr_hi = addr >> 14;
    uint16_t addr_lo = addr &= 0x3fff;
    uint8_t *page = mem->bank[addr_hi];
    return page[addr_lo];
}
