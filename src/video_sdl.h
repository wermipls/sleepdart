#pragma once

#include <stdint.h>
#include <stdbool.h>

void video_sdl_set_fps_limit(bool is_enabled);
bool video_sdl_get_fps_limit();
void video_sdl_set_fps(double fps);
void video_sdl_set_scale(int scale);
int video_sdl_get_scale();
void video_sdl_toggle_window_mode();
bool video_sdl_is_fullscreen();
int video_sdl_init(const char *title, int width, int height, int scale);
int video_sdl_draw_rgb24_buffer(void *pixeldata, size_t bytes);
