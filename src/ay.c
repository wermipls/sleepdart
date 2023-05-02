#include "ay.h"
#include <stdlib.h>
#include "machine.h"

static void ay_process_sample(AY_t *ay)
{
    ayumi_process_fast(&ay->ayumi);
    ayumi_remove_dc(&ay->ayumi);
    ay->buf[ay->buf_pos] = 0.3 * ay->ayumi.left;
    ay->buf[ay->buf_pos+1] = 0.3 * ay->ayumi.right;
    ay->buf_pos += 2;
}

int ay_init(AY_t *ay, struct Machine *ctx, int sample_rate, double clock)
{
    ay->samples_ratio = (double)ctx->timing.clock_hz / (double)sample_rate; 
    ay->samples_frame = (double)ctx->timing.t_frame / ay->samples_ratio;

    ay->buf_len = ay->samples_frame * 2;
    ay->buf_pos = 0;
    ay->last_write = 0;

    ayumi_configure(&ay->ayumi, 0, clock, sample_rate);
    ayumi_set_pan(&ay->ayumi, 0, 0.4, 1);
    ayumi_set_pan(&ay->ayumi, 1, 0.5, 1);
    ayumi_set_pan(&ay->ayumi, 2, 0.6, 1);

    ay->buf = malloc(ay->buf_len * sizeof(double));
    if (ay->buf == NULL) {
        return -1;
    }

    for (size_t i = 0; i < 16; i++) {
        ay->regs[i] = 0;
    }

    ay->ctx = ctx;
    return 0;
}

void ay_deinit(AY_t *ay)
{
    free(ay->buf);
    ay->buf = NULL;
}

void ay_reset(AY_t *ay)
{
    if (ay == NULL) return;

    for (size_t i = 0; i < 16; i++) {
        ay_write_address(ay, i);
        ay_write_data(ay, 0);
    }
    ay_write_address(ay, 0);
}

void ay_write_address(AY_t *ay, uint8_t value)
{
    if (ay == NULL) return;
    ay->address = value % 16;
}

void ay_write_data(AY_t *ay, uint8_t value)
{
    if (ay == NULL) return;

    uint8_t *r = ay->regs;
    r[ay->address] = value;

    switch (ay->address)
    {
    case 0:
    case 1:
        ayumi_set_tone(&ay->ayumi, 0, (r[1] << 8) | r[0]);
        break;
    case 2:
    case 3:
        ayumi_set_tone(&ay->ayumi, 1, (r[3] << 8) | r[2]);
        break;
    case 4:
    case 5:
        ayumi_set_tone(&ay->ayumi, 2, (r[5] << 8) | r[4]);
        break;
    case 6:
        ayumi_set_noise(&ay->ayumi, r[6]);
        break;
    case 7:
    case 8:
    case 9:
    case 10:
        ayumi_set_mixer(&ay->ayumi, 0, r[7] & 1, (r[7] >> 3) & 1, r[8] >> 4);
        ayumi_set_mixer(&ay->ayumi, 1, (r[7] >> 1) & 1, (r[7] >> 4) & 1, r[9] >> 4);
        ayumi_set_mixer(&ay->ayumi, 2, (r[7] >> 2) & 1, (r[7] >> 5) & 1, r[10] >> 4);
        ayumi_set_volume(&ay->ayumi, 0, r[8] & 0xf);
        ayumi_set_volume(&ay->ayumi, 1, r[9] & 0xf);
        ayumi_set_volume(&ay->ayumi, 2, r[10] & 0xf);
        break;
    case 11:
    case 12:
        ayumi_set_envelope(&ay->ayumi, (r[12] << 8) | r[11]);
        break;
    case 13:
        ayumi_set_envelope_shape(&ay->ayumi, r[13]);
        break;
    }

    uint64_t write = ay->ctx->cpu.cycles / ay->samples_ratio;
    for ( ; ay->last_write < write; ay->last_write++) {
        ay_process_sample(ay);
    }
}

uint8_t ay_read_data(AY_t *ay)
{
    return ay->regs[ay->address];
}

void ay_process_frame(AY_t *ay)
{
    for ( ; ay->last_write < ay->samples_frame; ay->last_write++) {
        ay_process_sample(ay);
    }

    ay->last_write = 0;
    ay->buf_pos = 0;
}
