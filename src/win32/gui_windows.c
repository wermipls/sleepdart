#include "gui_windows.h"
#include "resource.h"
#include <SDL2/SDL.h>
#include "../sleepdart_info.h"
#include "../video_sdl.h"
#include "../machine.h"
#include "../palette.h"
#include "../file.h"
#include "../unicode.h"

static WNDPROC sdl_wndproc;
static HMENU menu = NULL;
static HWND parent_window = NULL;

void windowscale_update_check()
{
    int scale = video_sdl_get_scale();
    for (int i = ID_WINDOWSCALE_1X; i <= ID_WINDOWSCALE_5X; i++) {
        CheckMenuItem(menu, i, MF_BYCOMMAND | MF_UNCHECKED);
    }

    CheckMenuItem(menu, scale-1 + ID_WINDOWSCALE_1X, MF_BYCOMMAND | MF_CHECKED);
}

void gui_windows_limit_fps_update_check()
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

void menu_palette_update_check()
{
    size_t palette = palette_get_index();
    for (size_t i = ID_PALETTE_BASE; i <= ID_PALETTE_BASE_END; i++) {
        CheckMenuItem(menu, i, MF_BYCOMMAND | MF_UNCHECKED);
    }

    CheckMenuItem(menu, palette + ID_PALETTE_BASE, MF_BYCOMMAND | MF_CHECKED);
}

void menu_palette_init()
{
    char **list = palette_list_get();
    if (list == NULL) {
        return;
    }

    size_t i = 0;
    while (list[i] != NULL && i <= ID_PALETTE_BASE_END-ID_PALETTE_BASE) {
        char *path = list[i];
        char *ext = file_get_extension(path);
        char path_noext[256];
        if (ext) {
            size_t len = ext - path;
            len = len > sizeof(path_noext) ? sizeof(path_noext) : len;
            strncpy(path_noext, path, len-1);
            path_noext[len-1] = 0;
            path = path_noext;
        }

        MENUITEMINFOA item;
        item.cbSize = sizeof(item);
        item.fMask = MIIM_STRING | MIIM_ID;
        item.wID = ID_PALETTE_BASE + i;
        item.dwTypeData = path;

        InsertMenuItemA(menu, ID_PALETTE_DEFAULT, false, &item);
        i++;
    }

    MENUITEMINFOA item;
    item.cbSize = sizeof(item);
    item.fMask = MIIM_FTYPE;
    item.fType = MFT_SEPARATOR;

    InsertMenuItemA(menu, ID_PALETTE_DEFAULT, false, &item);

    menu_palette_update_check();
}

void on_file_open(HWND hwnd)
{
    OPENFILENAMEW ofn;
    wchar_t file[2048];

    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd;
    ofn.lpstrFile = file;
    ofn.lpstrFile[0] = 0;
    ofn.nMaxFile = sizeof(file);
    ofn.lpstrFilter = L"All supported files\0*.tap;*.szx*\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrFileTitle = NULL;
    ofn.nMaxFileTitle = 0;
    ofn.lpstrInitialDir = NULL;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

    if (GetOpenFileNameW(&ofn) != TRUE) {
        return;
    }

    char *str = utf16_to_utf8(ofn.lpstrFile, NULL);
    if (str == NULL) {
        return;
    }
    machine_open_file(str);
    free(str);
}

void on_file_save(HWND hwnd)
{
    OPENFILENAMEW ofn;
    wchar_t file[2048];

    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd;
    ofn.lpstrFile = file;
    ofn.lpstrFile[0] = 0;
    ofn.nMaxFile = sizeof(file);
    ofn.lpstrFilter = L"SZX state\0*.szx\0";
    ofn.lpstrDefExt = L"szx";
    ofn.nFilterIndex = 1;
    ofn.lpstrFileTitle = NULL;
    ofn.nMaxFileTitle = 0;
    ofn.lpstrInitialDir = NULL;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT;

    if (GetSaveFileNameW(&ofn) != TRUE) {
        return;
    }

    char *str = utf16_to_utf8(ofn.lpstrFile, NULL);
    if (str == NULL) {
        return;
    }
    machine_save_file(str);
    free(str);
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
            machine_reset();
            return TRUE;
            break;
        case ID_FILE_OPEN:
            on_file_open(hwnd);
            return TRUE;
            break;
        case ID_FILE_SAVE:
            on_file_save(hwnd);
            return TRUE;
            break;
        case ID_FILE_QSAVE:
            machine_save_quick();
            return TRUE;
            break;
        case ID_FILE_QLOAD:
            machine_load_quick();
            return TRUE;
            break;
        case ID_FILE_QUIT:
            on_file_quit();
            return TRUE; 
            break;
        case ID_OPTIONS_LIMITFPS:
            video_sdl_set_fps_limit(!video_sdl_get_fps_limit());
            return TRUE;
            break;
        case ID_OPTIONS_MENUBAR:
            gui_windows_toggle_menubar();
            return TRUE;
            break;
        case ID_OPTIONS_FULLSCREEN:
            video_sdl_toggle_window_mode();
            fullscreen_update_check();
            return TRUE;
            break;
        case ID_PALETTE_DEFAULT:
            palette_set_default();
            menu_palette_update_check();
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

        WORD id = LOWORD(wparam);
        if (id >= ID_PALETTE_BASE && id <= ID_PALETTE_BASE_END) {
            palette_set_by_index(id-ID_PALETTE_BASE);
            menu_palette_update_check();
            return TRUE;
        }
    }

    return CallWindowProc(sdl_wndproc, hwnd, umsg, wparam, lparam);
}

void menu_init()
{
    // force SDL to resize window to compensate for the menu bar that we just added
    video_sdl_set_scale(video_sdl_get_scale());

    menu_palette_init();

    windowscale_update_check();
    gui_windows_limit_fps_update_check();
}

void gui_windows_hook_window(HWND hwnd)
{
    parent_window = hwnd;
    menu = LoadMenu(GetModuleHandleA(NULL), MAKEINTRESOURCE(IDR_MENU1));
    SetMenu(hwnd, menu);

    menu_init();

    sdl_wndproc = (WNDPROC)GetWindowLongPtr(hwnd, GWLP_WNDPROC);
    SetWindowLongPtr(hwnd, GWLP_WNDPROC, (LONG_PTR)wnd_proc);
}

void gui_windows_toggle_menubar()
{
    if (parent_window == NULL) return;

    HMENU current = GetMenu(parent_window);
    if (current == NULL) {
        SetMenu(parent_window, menu);
    } else {
        SetMenu(parent_window, NULL);
    }

    video_sdl_set_scale(video_sdl_get_scale());
}
