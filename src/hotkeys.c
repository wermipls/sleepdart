#include "hotkeys.h"
#include "machine.h"
#include "input_sdl.h"
#include "video_sdl.h"

void hotkeys_process()
{
    if (input_sdl_get_key_pressed(SDL_SCANCODE_INSERT)) {
        machine_toggle_tape_playback();
    }

    if (input_sdl_get_key_pressed(SDL_SCANCODE_F5)) {
        machine_save_quick();
    }

    if (input_sdl_get_key_pressed(SDL_SCANCODE_F7)) {
        machine_load_quick();
    }

    if (input_sdl_get_key_pressed(SDL_SCANCODE_F11)) {
        video_sdl_toggle_window_mode();
    }

    if (input_sdl_get_key_pressed(SDL_SCANCODE_F1)) {
        machine_reset();
    }

    if (input_sdl_get_key_pressed(SDL_SCANCODE_F4)) {
        video_sdl_set_fps_limit(!video_sdl_get_fps_limit());
    }

    if (input_sdl_get_key_pressed(SDL_SCANCODE_F8)) {
        video_sdl_toggle_menubar();
    }

    if (input_sdl_get_key(SDL_SCANCODE_LCTRL)) {
        if (input_sdl_get_key_pressed(SDL_SCANCODE_EQUALS)) {
            video_sdl_set_scale(video_sdl_get_scale() + 1);
        }

        if (input_sdl_get_key_pressed(SDL_SCANCODE_MINUS)) {
            video_sdl_set_scale(video_sdl_get_scale() - 1);
        }
    }
}
