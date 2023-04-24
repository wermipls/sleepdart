#pragma once

#include "z80.h"
#include "tape.h"
#include "memory.h"
#include "ula.h"
#include "ay.h"
#include "beeper.h"

enum MachineType
{
    MACHINE_ZX48K,
};

struct MachineTiming
{
    uint64_t clock_hz;

    unsigned int t_firstpx;
    unsigned int t_scanline;
    unsigned int t_screen;
    unsigned int t_frame;
    unsigned int t_eightpx;
    unsigned int t_int_hold;
};

typedef struct Machine {
    enum MachineType type;
    struct MachineTiming timing;

    Z80_t cpu;
    Memory_t memory;

    TapePlayer_t *tape_player;
    AY_t ay;
    Beeper_t beeper;

    uint64_t frames;
    bool reset_pending;
} Machine_t;

void machine_set_current(Machine_t *machine);
int machine_init(Machine_t *machine, enum MachineType type);
void machine_reset();
