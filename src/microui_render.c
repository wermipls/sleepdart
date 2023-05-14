#include "microui.h"
#include "file.h"
#include <SDL2/SDL.h>
#include <freetype2/ft2build.h>
#include FT_FREETYPE_H

static FT_Library ft;
static FT_Face face;
static SDL_Window *window = NULL;
static SDL_Renderer *renderer = NULL;
static int width = 600;
static int height = 600;

struct Glyph
{
    SDL_Texture *tex;
    int8_t adv_x;
    int8_t adv_y;
    int8_t offset_x;
    int8_t offset_y;
    int8_t w;
    int8_t h;
};

static struct Glyph cache[256] = { 0 };
static int glyph_height;

static int cache_glyph(FT_Face f, char c)
{
    struct Glyph cached = { 0 };
    FT_GlyphSlot slot = f->glyph;
    uint8_t cache_i = c;

    uint32_t idx = FT_Get_Char_Index(f, c);
    int err = FT_Load_Glyph(f, idx, FT_LOAD_NO_BITMAP);
    if (err) {
        goto notex;
    }

    err = FT_Render_Glyph(slot, FT_RENDER_MODE_NORMAL);
    if (err) {
        goto notex;
    }

    FT_Bitmap *bitmap = &slot->bitmap;
    SDL_Texture *tex = SDL_CreateTexture(
        renderer, 
        SDL_PIXELFORMAT_ARGB32,
        SDL_TEXTUREACCESS_STREAMING, bitmap->width, bitmap->rows);
    
    void *buf;
    int pitch;

    err = SDL_LockTexture(tex, NULL, &buf, &pitch);
    if (err) {
        goto notex;
    }

    uint32_t *px = buf;

    for (int i = 0; i < bitmap->pitch*bitmap->rows; i++) {
        px[i] = 0xFFFFFF00 | bitmap->buffer[i];
    }

    SDL_UnlockTexture(tex);

    SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);

    cached.tex = tex;
    cached.w = bitmap->width;
    cached.h = bitmap->rows;
notex:
    cached.offset_x = slot->bitmap_left;
    cached.offset_y = 0 - slot->bitmap_top;
    cached.adv_x = slot->advance.x / 64;
    cached.adv_y = slot->advance.y / 64;

    cache[cache_i] = cached;

    return 0;
}

int render_text_width(mu_Font font, const char *text, int len)
{
    int x = 0;

    while (*text && len) {
        uint8_t i = *text;
        x += cache[i].adv_x;

        text++;
        len--;
    }

    return x;
}

int render_text_height(mu_Font font)
{
    return glyph_height;
}

void render_text(mu_Font font, const char *text, mu_Vec2 pos, mu_Color color)
{
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    pos.y += face->size->metrics.ascender / 64;

    while (*text) {
        struct Glyph *g = &cache[(uint8_t)*text];
        SDL_Rect r = {
            .x = pos.x + g->offset_x,
            .y = pos.y + g->offset_y,
            .w = g->w,
            .h = g->h,
        };

        SDL_RenderCopy(renderer, g->tex, NULL, &r);

        pos.x += g->adv_x;
        pos.y += g->adv_y;

        text++;
    }
}

int render_init()
{
    if (renderer) return -1;

    int err = FT_Init_FreeType(&ft);
    if (err) {
      return -4;
    }

    char buf[2048];
    file_path_append(buf, file_get_basedir(), "font.ttf", sizeof(buf));
    err = FT_New_Face(ft, buf, 0, &face);
    if (err) {
        return -5;
    }

    err = FT_Set_Char_Size(face, 0, 12*64, 72, 72);
    if (err) {
        return -6;
    }

    window = SDL_CreateWindow(
        "sleepdart dbg",
        SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
        width, height, 0);

    if (!window) {
        return -2;
    }

    renderer = SDL_CreateRenderer(window, -1, 0);
    if (!renderer) {
        SDL_DestroyWindow(window);
        return -3;
    }

    for (int i = 0; i < 256; i++) {
        cache_glyph(face, i);
    }

    glyph_height = face->size->metrics.height / 64;

    return 0;
}

void render_deinit()
{
    if (renderer) {
        SDL_DestroyRenderer(renderer);
    }
    if (window) {
        SDL_DestroyWindow(window);
    }

    return;
}

void render_draw_rect(mu_Rect rect, mu_Color c)
{
    SDL_Rect r = { 
        .x = rect.x,
        .y = rect.y,
        .w = rect.w,
        .h = rect.h,
    };

    SDL_SetRenderDrawColor(renderer, c.r, c.g, c.b, c.a);
    SDL_RenderFillRect(renderer, &r);
}

void render_clear(mu_Color c)
{
    SDL_SetRenderDrawColor(renderer, c.r, c.g, c.b, c.a);
    SDL_RenderClear(renderer);
}

void render_present()
{
    SDL_RenderPresent(renderer);
}
