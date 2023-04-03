#pragma once

#include <stdint.h>

uint8_t io_port_write(uint16_t addr, uint8_t value);
uint8_t io_port_read(uint16_t addr, uint8_t *dest);
