#pragma once

#include <stdint.h>

typedef struct Beeper {
    double step;
    float dc_rate;
    float lp_rate;

    float accumulator;
    float dc;
    float lp;
    int last_write_is_high;
    int last_write_cycle;
    double x;

    size_t bufpos;
    size_t buflen;
    float *buf;
} Beeper_t;

struct Machine;

int beeper_init(Beeper_t *beeper, struct Machine *ctx, int sample_rate);
void beeper_deinit(Beeper_t *beeper);
void beeper_write(Beeper_t *beeper, int is_high, int cycle);
void beeper_process_frame(Beeper_t *beeper);
