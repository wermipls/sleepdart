#pragma once

#include <stdint.h>

void sound_sdl_init(int sample_rate);
void sound_sdl_queue(float *buf, size_t len);
