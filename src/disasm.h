#pragma once

#include <stdint.h>

/* returns a disassembled instruction string or NULL on error.
 * len gets set to instruction length in bytes.
 * user needs to free the string after use */
char *disasm_opcode(uint8_t *data, int *len, uint16_t pc);
