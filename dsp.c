#include "dsp.h"
#include <math.h>

#define M_PI 3.14159265358979323846

double dsp_normalize_freq(double freq, double sr)
{
    return freq * 2. * M_PI / sr; 
}

double dsp_derive_1pole_factor(double freq)
{
    // assumes normalized angular frequency
    // https://dsp.stackexchange.com/a/54088

    double y = 1. - cos(freq);
    return -y + sqrt(y*y + 2*y);
}

void dsp_mix_buffers(float *a, float *b, size_t len_a)
{
    for (size_t i = 0; i < len_a; i++) {
        a[i] += b[i];
    }
}

void dsp_mix_buffers_mono_to_stereo(float *a, float *b_mono, size_t len_a)
{
    for (size_t i = 0; i < len_a; i += 2) {
        a[i]   += *b_mono;
        a[i+1] += *b_mono;
        b_mono++;
    }
}
