#pragma once

#include "z80.h"
#include "tape.h"
#include "memory.h"

enum MachineType
{
    MACHINE_ZX48K,
};

typedef struct Machine {
    enum MachineType type;
    Z80_t cpu;
    Memory_t memory;

    TapePlayer_t *tape_player;

    bool reset_pending;
} Machine_t;

void machine_set_current(Machine_t *machine);
int machine_init(Machine_t *machine, enum MachineType type);
void machine_reset();
