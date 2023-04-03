#include "gui_windows.h"
#include "resource.h"
#include <SDL2/SDL.h>
#include "../sleepdart_info.h"
#include "../video_sdl.h"
#include "../z80.h"

WNDPROC sdl_wndproc;
HMENU menu;

void windowscale_update_check()
{
    int scale = video_sdl_get_scale();
    for (int i = ID_WINDOWSCALE_1X; i <= ID_WINDOWSCALE_5X; i++) {
        CheckMenuItem(menu, i, MF_BYCOMMAND | MF_UNCHECKED);
    }

    CheckMenuItem(menu, scale-1 + ID_WINDOWSCALE_1X, MF_BYCOMMAND | MF_CHECKED);
}

void limit_fps_update_check()
{
    bool is_on = video_sdl_get_fps_limit();
    CheckMenuItem(menu, ID_OPTIONS_LIMITFPS, MF_BYCOMMAND | MF_CHECKED*is_on);
}

void fullscreen_update_check()
{
    bool is_on = video_sdl_is_fullscreen();
    CheckMenuItem(menu, ID_OPTIONS_FULLSCREEN, MF_BYCOMMAND | MF_CHECKED*is_on);
}

void on_file_quit()
{
    SDL_Event e;
    e.type = SDL_QUIT;
    SDL_PushEvent(&e);
}

LRESULT CALLBACK wnd_proc(HWND hwnd, UINT umsg, WPARAM wparam, LPARAM lparam)
{
    switch (umsg)
    {
    case WM_COMMAND: 
        switch(LOWORD(wparam)) 
        { 
        case ID_HELP_ABOUT: 
            MessageBoxA(
                hwnd, 
                SLEEPDART_NAME " version " SLEEPDART_VERSION "\n"
                "Compiled on " __DATE__ "\n\n"
                SLEEPDART_DESCRIPTION "\n\n"
                SLEEPDART_REPO,
                "About " SLEEPDART_NAME, // title
                MB_OK | MB_ICONINFORMATION);
            return TRUE;
        case ID_MACHINE_RESET:
            cpu_init(&cpu);
            return TRUE;
            break;
        case ID_FILE_QUIT:
            on_file_quit();
            return TRUE; 
            break;
        case ID_OPTIONS_LIMITFPS:
            video_sdl_set_fps_limit(!video_sdl_get_fps_limit());
            limit_fps_update_check();
            return TRUE;
            break;
        case ID_OPTIONS_FULLSCREEN:
            video_sdl_toggle_window_mode();
            fullscreen_update_check();
            return TRUE;
            break;

        case ID_WINDOWSCALE_1X:
        case ID_WINDOWSCALE_2X:
        case ID_WINDOWSCALE_3X:
        case ID_WINDOWSCALE_4X:
        case ID_WINDOWSCALE_5X: ;
            int scale = LOWORD(wparam) - ID_WINDOWSCALE_1X + 1;
            video_sdl_set_scale(scale);
            windowscale_update_check();
            return TRUE;
            break;
        }
    }

    return CallWindowProc(sdl_wndproc, hwnd, umsg, wparam, lparam);
}

void menu_init(HWND hwnd)
{
    video_sdl_set_scale(video_sdl_get_scale());

    windowscale_update_check();
    limit_fps_update_check();
}

void gui_windows_hook_window(HWND hwnd)
{
    menu = LoadMenu(GetModuleHandleA(NULL), MAKEINTRESOURCE(IDR_MENU1));
    SetMenu(hwnd, menu);

    menu_init(hwnd);

    sdl_wndproc = (WNDPROC)GetWindowLongPtr(hwnd, GWLP_WNDPROC);
    SetWindowLongPtr(hwnd, GWLP_WNDPROC, (LONG_PTR)wnd_proc);
}
