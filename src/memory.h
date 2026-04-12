#pragma once

#include <stdint.h>

typedef struct Memory {
    uint8_t rom[0x4000*2];
    uint8_t ram[0x4000*8];
    union {
        struct {
            uint8_t *bank_0000;
            uint8_t *bank_4000;
            uint8_t *bank_8000;
            uint8_t *bank_c000;
        };
        uint8_t *bank[4];
    };
} Memory_t;

struct Machine;

void memory_init(Memory_t *mem);
int memory_load_rom(Memory_t *mem, char path[]);
uint8_t memory_write(struct Machine *ctx, uint16_t addr, uint8_t value);
uint8_t memory_read(struct Machine *ctx, uint16_t addr, uint8_t *dest);
uint8_t memory_bus_peek(Memory_t *mem, uint16_t addr);
