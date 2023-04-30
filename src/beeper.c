#include "beeper.h"
#include <stdlib.h>
#include <limits.h>
#include <math.h>
#include "machine.h"
#include "dsp.h"

static float beeper_process_dc_lp(Beeper_t *b, float value)
{
    b->dc += b->dc_rate * (value - b->dc);
    value -= b->dc;

    b->lp += b->lp_rate * (value - b->lp);
    return b->lp;
}

static void beeper_process_loop(Beeper_t *b, int cycles)
{
    int delta = cycles ? cycles - b->last_write_cycle : INT_MAX;
    while (delta > 0) {
        if (b->bufpos >= b->buflen) {
            return;
        }

        int xr = ceil(b->x);
        if (delta < xr) {
            b->accumulator += delta * b->last_write_is_high;
            b->x -= delta;
            delta = 0;
        } else {
            b->accumulator += xr * b->last_write_is_high;
            delta -= xr;
            b->x -= xr;
        }

        if (b->x <= 0) {
            b->buf[b->bufpos] = b->accumulator / b->step;
            b->buf[b->bufpos] = beeper_process_dc_lp(b, b->buf[b->bufpos]) * 0.2;
            b->accumulator = 0;
            b->x += b->step;
            b->bufpos++;
        }
    }
}

int beeper_init(Beeper_t *beeper, struct Machine *ctx, int sample_rate)
{
    beeper->step = (double)ctx->timing.clock_hz / (double)sample_rate;
    beeper->buflen = (double)ctx->timing.t_frame / beeper->step;

    beeper->buf = malloc(sizeof(*beeper->buf) * beeper->buflen);
    if (beeper->buf == NULL) {
        return -1;
    }

    beeper->bufpos = 0;
    beeper->last_write_cycle = 0;
    beeper->last_write_is_high = 0;
    beeper->accumulator = 0;
    beeper->dc = 0;
    beeper->lp = 0;
    beeper->x = beeper->step;

    beeper->dc_rate = dsp_derive_1pole_factor(dsp_normalize_freq(30, sample_rate));
    beeper->lp_rate = dsp_derive_1pole_factor(dsp_normalize_freq(10000, sample_rate));

    return 0;
}

void beeper_deinit(Beeper_t *beeper)
{
    if (beeper == NULL) return;

    if (beeper->buf) {
        free(beeper->buf);
    }
}

void beeper_write(Beeper_t *beeper, int is_high, int cycle)
{
    beeper_process_loop(beeper, cycle);
    beeper->last_write_cycle = cycle;
    beeper->last_write_is_high = is_high;
}

void beeper_process_frame(Beeper_t *beeper)
{
    beeper_process_loop(beeper, 0);
    beeper->last_write_cycle = 0;
    beeper->bufpos = 0;
}
