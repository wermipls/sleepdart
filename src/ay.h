#pragma once

#include <stdint.h>
#include <stddef.h>
#include "../ayumi/ayumi.h"

struct Machine;

typedef struct AY
{
    int address;
    uint8_t regs[16];

    double samples_ratio;
    uint64_t samples_frame;
    uint64_t last_write;
    struct ayumi ayumi;
    size_t buf_len;
    size_t buf_pos;
    float *buf;
    struct Machine *ctx;
} AY_t;

AY_t *ay_init(struct Machine *ctx, int sample_rate, double clock);
void ay_deinit(AY_t *ay);
void ay_reset(AY_t *ay);
void ay_write_address(AY_t *ay, uint8_t value);
void ay_write_data(AY_t *ay, uint8_t value);
uint8_t ay_read_data(AY_t *ay);
void ay_process_frame(AY_t *ay);
