#pragma once

#include <stdint.h>

struct Machine;

uint8_t io_port_write(struct Machine *ctx, uint16_t addr, uint8_t value);
uint8_t io_port_read(struct Machine *ctx, uint16_t addr, uint8_t *dest);
