#pragma once

#include <stdint.h>

int video_sdl_init(const char *title, int width, int height, int scale);
int video_sdl_draw_rgb24_buffer(void *pixeldata, size_t bytes);
