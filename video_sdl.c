#include "video_sdl.h"

#include <SDL2/SDL.h>
#include "log.h"

int window_width, window_height;
int buffer_width, buffer_height;

SDL_Renderer *renderer;
SDL_Texture *texture;

void sdl_log_error(const char msg[])
{
    dlog(LOG_ERR, "%s: %s\n", msg, SDL_GetError());
}

int video_sdl_init(const char *title, int width, int height, int scale)
{
    if (SDL_Init(SDL_INIT_VIDEO) < 0)
    {
        sdl_log_error("SDL_Init");
        return 1;
    }

    buffer_width = width;
    buffer_height = height;

    window_width = width * scale;
    window_height = height * scale;

    SDL_Window *window = SDL_CreateWindow(
        title, 
        SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 
        window_width, window_height, 
        SDL_WINDOW_SHOWN);

    if (window == NULL)
    {
        sdl_log_error("SDL_CreateWindow");
        SDL_Quit();
        return 2;
    }

    renderer = SDL_CreateRenderer(
        window,
        -1, // rendering driver index
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);

    if (renderer == NULL)
    {
        sdl_log_error("SDL_CreateRenderer");
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 3;
    }

    texture = SDL_CreateTexture(
        renderer, 
        SDL_PIXELFORMAT_RGB24,
        SDL_TEXTUREACCESS_STREAMING,
        buffer_width, buffer_height);

    if (texture == NULL)
    {
        sdl_log_error("SDL_CreateTextureFromSurface");
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 4;
    }
    
    return 0;
}

int video_sdl_draw_rgb24_buffer(void *pixeldata, size_t bytes)
{
    int err = SDL_RenderClear(renderer);
    if (err) sdl_log_error("SDL_RenderClear");

    void *pixels;
    int pitch; 

    err = SDL_LockTexture(texture, NULL, &pixels, &pitch);
    if (err) sdl_log_error("SDL_LockTexture");

    memcpy(pixels, pixeldata, 3*buffer_height*buffer_width);
    SDL_UnlockTexture(texture);

    err = SDL_RenderCopy(renderer, texture, NULL, NULL);
    if (err) sdl_log_error("SDL_RenderCopy");

    SDL_RenderPresent(renderer);

    int quit = 0;


    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        switch (e.type)
        {
        case SDL_QUIT:
            quit = 1;
        }
    }

    SDL_Delay(20);

    return quit;
}
