#pragma once

#include <stdint.h>
#include <stddef.h>

double dsp_normalize_freq(double freq, double sr);
double dsp_derive_1pole_factor(double freq);
void dsp_mix_buffers(float *a, float *b, size_t len_a);
void dsp_mix_buffers_mono_to_stereo(float *a, float *b_mono, size_t len_a);
