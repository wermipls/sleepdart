#pragma once

#include "microui.h"

int render_text_width(mu_Font font, const char *text, int len);
int render_text_height(mu_Font font);
void render_text(mu_Font font, const char *text, mu_Vec2 pos, mu_Color color);
int render_init();
void render_deinit();
void render_draw_rect(mu_Rect rect, mu_Color c);
void render_clear(mu_Color c);
void render_present();
