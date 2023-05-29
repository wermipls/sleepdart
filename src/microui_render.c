#include "microui_render.h"

#include "microui.h"
#include "file.h"
#include "log.h"
#include <SDL2/SDL.h>
#include <zlib.h>
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

static SDL_Texture *icons_atlas;

struct Icon
{
    SDL_Rect src;
    mu_Color color;
};

static struct Icon icons[16];

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
    int size = bitmap->pitch*bitmap->rows;

    for (int i = 0; i < size; i++) {
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

SDL_Texture *load_rgba32_zlib_texture(const char *path, int width, int height)
{
    int64_t fsize = file_get_size(path);
    if (fsize <= 0) return NULL;
    
    uint8_t src[fsize];
    FILE *f = fopen(path, "rb");
    fread(src, 1, fsize, f);
    fclose(f);

    SDL_Texture *tex = SDL_CreateTexture(
        renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STREAMING,
        width, height);

    if (!tex) return NULL;

    void *buf;
    int pitch;

    SDL_LockTexture(tex, NULL, &buf, &pitch);
    uLongf len = pitch * height;

    uncompress(buf, &len, src, fsize);
    SDL_UnlockTexture(tex);
    SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);

    return tex;
}

int render_text_width(mu_Font font, const char *text, int len)
{
    if (!text) return 0;

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
    if (!text) return;

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    pos.y += face->size->metrics.ascender / 64;

    while (*text) {
        struct Glyph *g = &cache[(uint8_t)*text];
        SDL_Rect r = {
            .x = pos.x + g->offset_x,
            .y = pos.y + g->offset_y,
            .w = g->w,
            .h = g->h,
        };

        SDL_SetTextureColorMod(g->tex, color.r, color.g, color.b);
        SDL_SetTextureAlphaMod(g->tex, color.a);
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
        dlog(LOG_ERR, "%s: failed to initialize FreeType", __func__);
        return -4;
    }

    char buf[2048];
    file_path_append(buf, file_get_basedir(), "assets/font.ttf", sizeof(buf));
    err = FT_New_Face(ft, buf, 0, &face);
    if (err) {
        dlog(LOG_ERR, "%s: failed to load face \"%s\"", __func__, buf);
        return -5;
    }

    err = FT_Set_Char_Size(face, 0, 12*64, 72, 72);
    if (err) {
        dlog(LOG_ERR, "%s: failed to set character size", __func__);
        return -6;
    }

    window = SDL_CreateWindow(
        "sleepdart dbg",
        SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
        width, height, 0);

    if (!window) {
        dlog(LOG_ERR, "%s: failed to create window", __func__);
        return -2;
    }

    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");

    renderer = SDL_CreateRenderer(window, -1, 0);
    if (!renderer) {
        SDL_DestroyWindow(window);
        dlog(LOG_ERR, "%s: failed to create renderer", __func__, buf);
        return -3;
    }

    for (int i = 0; i < 256; i++) {
        cache_glyph(face, i);
    }

    glyph_height = face->size->metrics.height / 64;

    file_path_append(buf, file_get_basedir(), "assets/icons.raw.zlib", sizeof(buf));
    icons_atlas = load_rgba32_zlib_texture(buf, 1024, 64);
    if (!icons_atlas) {
        dlog(LOG_WARN, "%s: failed to load \"%s\"", __func__, buf);
    }

    for (int i = 1; i < 1024/64; i++) {
        icons[i].src = (SDL_Rect) { (i-1)*64, 0, 64, 64 };
        icons[i].color = mu_color(0, 0, 0, 0);
    }

    icons[DBGICON_CURRENT].color = (mu_Color) { 255, 192, 50, 255 };
    icons[DBGICON_BREAKPOINT].color = (mu_Color) { 238, 49, 116, 255 };

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

void render_draw_icon(int id, mu_Rect rect, mu_Color c)
{
    SDL_Rect r = { 
        .x = rect.x,
        .y = rect.y,
        .w = rect.w,
        .h = rect.h,
    };

    SDL_Rect *src = &icons[id].src;
    mu_Color colors[2] = {
        c,
        icons[id].color,
    };

    c = colors[!(!colors[1].a)];

    if (!icons_atlas) {
        render_draw_rect(rect, c);
        return;
    }

    SDL_SetTextureColorMod(icons_atlas, c.r, c.g, c.b);
    SDL_SetTextureAlphaMod(icons_atlas, c.a);
    SDL_RenderCopy(renderer, icons_atlas, src, &r);
}

void render_clip_rect(mu_Rect rect)
{
    SDL_Rect r = { 
        .x = rect.x,
        .y = rect.y,
        .w = rect.w,
        .h = rect.h,
    };

    SDL_RenderSetClipRect(renderer, &r);
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

uint32_t render_get_window_id()
{
    return SDL_GetWindowID(window);
}
