#include "palette.h"
#include "file.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char **palette_list = NULL;
size_t palette_current = 0;
bool palette_changed = false;

Palette_t *palette_load(const char *path)
{
    if (path == NULL) {
        return NULL;
    }

    int64_t size = file_get_size(path);
    if (size <= 0) {
        return NULL;
    }

    // its just a palette bro...
    if (size > 4096) {
        return NULL;
    }

    // not rgb24
    if ((size % 3) != 0) {
        return NULL;
    }

    size_t colors = size / 3;

    FILE *f = fopen_utf8(path, "rb");
    if (f == NULL) {
        return NULL;
    }

    Palette_t p;
    p.colors = colors;
    p.color = malloc(sizeof(struct PaletteColor) * colors);
    if (p.color == NULL) {
        fclose(f);
        return NULL;
    }

    for (size_t i = 0; i < colors; i++) {
        struct PaletteColor c;

        size_t bytes = fread(&c.r, 1, 1, f);
        if (bytes == 0) {
            free(p.color);
            fclose(f);
            return NULL;
        }

        bytes = fread(&c.g, 1, 1, f);
        if (bytes == 0) {
            free(p.color);
            fclose(f);
            return NULL;
        }

        bytes = fread(&c.b, 1, 1, f);
        if (bytes == 0) {
            free(p.color);
            fclose(f);
            return NULL;
        }

        p.color[i] = c;
    }

    fclose(f);

    Palette_t *result = malloc(sizeof(Palette_t));
    if (result == NULL) {
        free(p.color);
        return NULL;
    }

    *result = p;
    return result;
}

Palette_t *palette_load_current()
{
    if (palette_list == NULL) {
        return NULL;
    }

    char path[2048];
    int err = file_path_append(path, file_get_basedir(), "palettes", sizeof(path));
    if (err) {
        return NULL;
    }
    err = file_path_append(path, path, palette_list[palette_current], sizeof(path));
    if (err) {
        return NULL;
    }

    Palette_t *p = palette_load(path);
    if (p == NULL) {
        return NULL;
    }

    return p;
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
    char path[2048];
    int err = file_path_append(path, file_get_basedir(), "palettes", sizeof(path));
    if (err) {
        return;
    }
    palette_list = file_list_directory_files(path);
}

void palette_list_free()
{
    if (palette_list) {
        file_free_list(palette_list);
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
