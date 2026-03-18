#include "video_sdl.h"

#include <SDL3/SDL.h>
#include <math.h>
#include "log.h"
#include <stdio.h>

#if defined(_WIN32) && defined(PLATFORM_WIN32)
    #include "win32/gui_windows.h"
#endif

struct Frametime
{
    uint64_t frames;
    uint64_t time;
};

int window_width, window_height;
int buffer_width, buffer_height;
int window_scale;

SDL_Window *window;
SDL_Renderer *renderer;
SDL_Texture *texture;

char title_base[256];
char title_buf[256];

bool fullscreen = false;
bool limit_fps = true;
double target_ticks_frame = 0;

void video_sdl_set_fps_limit(bool is_enabled)
{
    limit_fps = is_enabled;
#if defined(_WIN32) && defined(PLATFORM_WIN32)
    gui_windows_limit_fps_update_check();
#endif
}

bool video_sdl_get_fps_limit()
{
    return limit_fps;
}

void video_sdl_set_fps(double fps)
{
    target_ticks_frame = 1000.0 / fps;
}

void video_sdl_set_scale(int scale)
{
    window_scale = scale;
    window_width = buffer_width * scale;
    window_height = buffer_height * scale;

    SDL_SetWindowSize(window, window_width, window_height);
}

int video_sdl_get_scale()
{
    return window_scale;
}

void video_sdl_toggle_window_mode()
{
    fullscreen = !fullscreen;
    SDL_SetWindowFullscreen(window, fullscreen);
}

bool video_sdl_is_fullscreen()
{
    return fullscreen;
}

void sdl_set_window_title_fps()
{
    static struct Frametime h[5] = { 0 };
    static const uint16_t update_interval = 1000;
    static uint16_t frames = 0;
    static uint64_t ticks_old = 0;
    uint64_t ticks = SDL_GetTicks();

    frames++;
    uint16_t time = ticks - ticks_old;
    if (time >= update_interval) {
        struct Frametime hnew;
        hnew.frames = frames;
        hnew.time = time;
        ticks_old = ticks;
        frames = 0;

        size_t i;
        for (i = 1; i < sizeof(h) / sizeof(struct Frametime); i++) {
            h[i-1] = h[i];
        }
        h[i-1] = hnew;

        float ftotal = 0;
        float ttotal = 0;
        for (i = 0; i < sizeof(h) / sizeof(struct Frametime); i++) {
            ftotal += h[i].frames * powf(2, i+1);
            ttotal += h[i].time   * powf(2, i+1);
        }

        float fps = ftotal / (ttotal / 1000.0f);
        snprintf(title_buf, sizeof(title_buf), "%s [%.2f FPS]", title_base, fps);
        SDL_SetWindowTitle(window, title_buf);
    }
}

void sdl_synchronize_fps()
{
    static uint64_t ticks_next = 0;
    static double error = 0;
    int ticks_frame_flr = floor(target_ticks_frame);
    uint64_t ticks = SDL_GetTicks();

    if (!limit_fps || (ticks_next < ticks - ticks_frame_flr - 1)) {
        ticks_next = ticks;
        return;
    }

    if (ticks_next > ticks) {
        int delay = ticks_next - ticks;
        SDL_Delay(delay);
    }

    error += target_ticks_frame - (double)ticks_frame_flr;
    ticks_next += ticks_frame_flr;
    if (error >= 1) {
        error -= 1;
        ticks_next += 1;
    }
}

void video_sdl_toggle_menubar()
{
#if defined(_WIN32) && defined(PLATFORM_WIN32)
    gui_windows_toggle_menubar();
#endif
}

void sdl_log_error(const char msg[])
{
    dlog(LOG_ERR, "%s: %s", msg, SDL_GetError());
}

int video_sdl_init(const char *title, int width, int height, int scale)
{
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        sdl_log_error("Failed to initialize the video subsystem");
        return 1;
    }

    buffer_width = width;
    buffer_height = height;

    window_scale = scale;
    window_width = width * scale;
    window_height = height * scale;

    strncpy(title_base, title, sizeof(title_base)-1);

    if (!SDL_CreateWindowAndRenderer(
        title,
        window_width,
        window_height,
        SDL_WINDOW_HIGH_PIXEL_DENSITY,
        &window,
        &renderer
    )) {
        sdl_log_error("Failed to create window");
        SDL_Quit();
        return 2;
    }

    SDL_SetRenderLogicalPresentation(
        renderer, buffer_width, buffer_height, SDL_LOGICAL_PRESENTATION_LETTERBOX);

    texture = SDL_CreateTexture(
        renderer, 
        SDL_PIXELFORMAT_RGB24,
        SDL_TEXTUREACCESS_STREAMING,
        buffer_width, buffer_height);

    if (!texture) {
        sdl_log_error("Failed to create texture");
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 4;
    }

    SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_PIXELART);

#if defined(_WIN32) && defined(PLATFORM_WIN32)
    HWND hwnd = (HWND)SDL_GetPointerProperty(SDL_GetWindowProperties(window), SDL_PROP_WINDOW_WIN32_HWND_POINTER, NULL);
    if (hwnd) {
        gui_windows_hook_window(hwnd);
    } else {
        sdl_log_error("Failed to get HWND of the window");
    }
#endif
    
    return 0;
}

int video_sdl_draw_rgb24_buffer(void *pixeldata, size_t bytes)
{
    if (!SDL_RenderClear(renderer)) {
        sdl_log_error("SDL_RenderClear");
    }

    void *pixels;
    int pitch; 

    static uint64_t ticks_last = 0;
    uint64_t ticks = SDL_GetTicks();
    if (ticks - ticks_last >= 10) {
        // i think it's fairly safe to assume no one truly needs
        // the mf window to update more often than every 100hz
        // with uncapped fps lol
        ticks_last = ticks;

        if (!SDL_LockTexture(texture, NULL, &pixels, &pitch)) {
            sdl_log_error("SDL_LockTexture");
        }

        size_t dstbytes = 3*buffer_height*buffer_width;
        if (dstbytes != bytes) {
            dlog(LOG_ERR, "%s: dstbytes != bytes", __func__);
            return -1;
        }

        memcpy(pixels, pixeldata, dstbytes);
        SDL_UnlockTexture(texture);

        if (!SDL_RenderTexture(renderer, texture, NULL, NULL)) {
            sdl_log_error("SDL_RenderCopy");
        }

        SDL_RenderPresent(renderer);
    }

    sdl_set_window_title_fps();
    sdl_synchronize_fps();

    return 0;
}
