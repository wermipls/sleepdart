#include "palette.h"
#include "file.h"
#include <stdio.h>
#include <stdlib.h>

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

    FILE *f = fopen(path, "rb");
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

void palette_free(Palette_t *palette)
{
    if (palette) {
        if (palette->color) {
            free(palette->color);
        }
        free(palette);
    }
}
