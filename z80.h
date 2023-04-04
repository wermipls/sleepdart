#pragma once

#include <stdint.h>
#include <stdbool.h>

struct Z80RegsMain {
    union {
        uint16_t af;
        struct {
            // TRUST ME BRO THIS IS THE *BEST* WAY TO DO IT
            // NINTENDO! HIRE THIS MAN
            // #CLEANCODE #KISS
            union {
                uint8_t f;
                struct { 
                    bool c  : 1;
                    bool n  : 1;
                    bool pv : 1;
                    bool y  : 1;
                    bool h  : 1;
                    bool x  : 1; 
                    bool z  : 1;
                    bool s  : 1;
                } flags;
            };
            uint8_t a;
        };
    };

    union {
        uint16_t bc;
        struct {
            uint8_t c;
            uint8_t b;
        };
    };

    union {
        uint16_t de;
        struct {
            uint8_t e;
            uint8_t d;
        };
    };

    union {
        uint16_t hl;
        struct {
            uint8_t l;
            uint8_t h;
        };
    };
};

struct Z80Regs {
    struct Z80RegsMain main;
    struct Z80RegsMain alt;

    union {
        uint16_t ix;
        struct {
            uint8_t ixl;
            uint8_t ixh;
        };
    };

    union {
        uint16_t iy;
        struct {
            uint8_t iyl;
            uint8_t iyh;
        };
    };

    uint8_t i;
    uint8_t r;
    uint16_t sp;
    uint16_t pc;
    bool iff1, iff2;
    uint8_t im;
};

typedef struct Z80 {
    struct Z80Regs regs;
    uint64_t cycles;
    enum PrefixState {
        STATE_NOPREFIX,
        STATE_DD,
        STATE_FD,
    } prefix_state;
    bool interrupt_pending;
    bool halt_resume;
} Z80_t;

extern Z80_t cpu;

void cpu_init(Z80_t *cpu);
void cpu_fire_interrupt(Z80_t *cpu);
int cpu_do_cycles(Z80_t *cpu);
