#pragma once

#include <stdint.h>

char *disassemble_opcode(uint8_t *data, int *len, uint16_t pc);
