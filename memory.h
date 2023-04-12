#pragma once

#include <stdint.h>

typedef struct Memory {
    uint8_t bus[0x10000];
} Memory_t;

void memory_init(Memory_t *mem);
int memory_load_rom_16k(Memory_t *mem, char path[]);
uint8_t memory_write(uint8_t *bus, uint16_t addr, uint8_t value, uint64_t cycle);
uint8_t memory_read(uint8_t *bus, uint16_t addr, uint8_t *dest, uint64_t cycle);
uint8_t memory_bus_peek(uint8_t *bus, uint16_t addr);
