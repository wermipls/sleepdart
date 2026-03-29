#include "palette.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <physfs.h>

char **palette_list = NULL;
size_t palette_current = 0;
bool palette_changed = false;

Palette_t *palette_load(const char *path)
{
    if (path == NULL) {
        return NULL;
    }

    PHYSFS_File *f = PHYSFS_openRead(path);
    if (!f) {
        return NULL;
    }

    Palette_t *p = malloc(sizeof(Palette_t));
    if (!p) {
        PHYSFS_close(f);
        return NULL;
    }

    p->colors = 16;
    p->color = malloc(sizeof(struct PaletteColor) * p->colors);
    if (!p->color) {
        PHYSFS_close(f);
        return NULL;
    }

    for (size_t i = 0; i < p->colors; i++) {
        PHYSFS_uint64 bytes = PHYSFS_readBytes(f, &p->color[i], 3);
        if (bytes != 3) {
            palette_free(p);
            PHYSFS_close(f);
            return NULL;
        }
    }

    PHYSFS_close(f);
    return p;
}

Palette_t *palette_load_current()
{
    if (palette_list == NULL) {
        return NULL;
    }

    char path[2048];
    snprintf(path, sizeof(path), "palettes/%s", palette_list[palette_current]);

    return palette_load(path);
}

void palette_free(Palette_t *palette)
{
    if (palette) {
        if (palette->color) {
            free(palette->color);
        }
        free(palette);
    }
}

void palette_list_init()
{
    palette_list = PHYSFS_enumerateFiles("palettes/");
}

void palette_list_free()
{
    if (palette_list) {
        PHYSFS_freeList(palette_list);
        palette_list = NULL;
    }
}

char **palette_list_get()
{
    return palette_list;
}

void palette_set_by_index(size_t index)
{
    palette_current = index;
    palette_changed = true;
}

void palette_set_by_name(const char *name)
{
    if (palette_list == NULL) {
        return;
    }

    for (size_t i = 0; palette_list[i] != NULL; i++) {
        int result = strcmp(name, palette_list[i]);
        if (result == 0) {
            palette_current = i;
            palette_changed = true;
        }
    }
}

void palette_set_default()
{
    if (palette_list == NULL) {
        return;
    }

    palette_set_by_name("default.raw");
}

size_t palette_get_index()
{
    return palette_current;
}

const char *palette_get_name()
{
    if (palette_list == NULL) {
        return NULL;
    }

    return palette_list[palette_current];
}

bool palette_has_changed()
{
    if (palette_changed) {
        palette_changed = false;
        return true;
    }
    return false;
}
