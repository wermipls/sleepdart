#pragma once

#include <stddef.h>

void audio_sdl_init(int sample_rate);
void audio_sdl_queue(float *buf, size_t len);
