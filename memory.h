#pragma once

#include <stdint.h>

void memory_init();
int memory_load_rom_16k(char path[]);
uint8_t memory_write(uint16_t addr, uint8_t value, uint64_t cycle);
uint8_t memory_read(uint16_t addr, uint8_t *dest, uint64_t cycle);
uint8_t memory_bus_peek(uint16_t addr);
