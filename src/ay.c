#include "ay.h"
#include <stdlib.h>
#include "machine.h"

static void ay_process_sample(AY_t *ay)
{
    ayumi_process(&ay->ayumi);
    ayumi_remove_dc(&ay->ayumi);
    ay->buf[ay->buf_pos] = 0.3 * ay->ayumi.left;
    ay->buf[ay->buf_pos+1] = 0.3 * ay->ayumi.right;
    ay->buf_pos += 2;
}

static void ay_process_sample_fast(AY_t *ay)
{
    ayumi_process_fast(&ay->ayumi);
    ayumi_remove_dc(&ay->ayumi);
    ay->buf[ay->buf_pos] = 0.3 * ay->ayumi.left;
    ay->buf[ay->buf_pos+1] = 0.3 * ay->ayumi.right;
    ay->buf_pos += 2;
}

AY_t *ay_init(struct Machine *ctx, int sample_rate, double clock)
{
    AY_t *ay = calloc(sizeof(AY_t), 1);
    if (ay == NULL) {
        return NULL;
    }

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
        free(ay);
        return NULL;
    }

    for (size_t i = 0; i < 16; i++) {
        ay->regs[i] = 0;
    }

    ay->ctx = ctx;
    return ay;
}

void ay_deinit(AY_t *ay)
{
    if (ay == NULL) return;

    free(ay->buf);
    free(ay);
}

static inline double clamp(double a, double min, double max)
{
    a = (a < min) ? min : a;
    a = (a > max) ? max : a;
    return a;
}

void ay_set_pan(AY_t *ay, double a, double b, double c, int equal_power)
{
    if (ay == NULL) return;

    ayumi_set_pan(&ay->ayumi, 0, clamp(a, 0.0, 1.0), equal_power);
    ayumi_set_pan(&ay->ayumi, 1, clamp(b, 0.0, 1.0), equal_power);
    ayumi_set_pan(&ay->ayumi, 2, clamp(c, 0.0, 1.0), equal_power);
}

void ay_set_quality(AY_t *ay, int high_quality)
{
    if (ay == NULL) return;

    ay->is_hq = high_quality;
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

    if (ay->is_hq) {
        for ( ; ay->last_write < write; ay->last_write++) {
            ay_process_sample(ay);
        }
    } else {
        for ( ; ay->last_write < write; ay->last_write++) {
            ay_process_sample_fast(ay);
        }
    }
}

uint8_t ay_read_data(AY_t *ay)
{
    if (ay == NULL) return 0;

    return ay->regs[ay->address];
}

void ay_process_frame(AY_t *ay)
{
    if (ay == NULL) return;

    if (ay->is_hq) {
        for ( ; ay->last_write < ay->samples_frame; ay->last_write++) {
            ay_process_sample(ay);
        }
    } else {
        for ( ; ay->last_write < ay->samples_frame; ay->last_write++) {
            ay_process_sample_fast(ay);
        }
    }

    ay->last_write = 0;
    ay->buf_pos = 0;
}
