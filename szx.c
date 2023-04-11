#include "szx.h"
#include <stdio.h>
#include <string.h>

const char header_magic[4] = "ZXST";

bool szx_is_valid_file(char *path)
{
    FILE *f = fopen(path, "rb");
    if (f == NULL) {
        return false;
    }

    char magic[4];
    size_t bytes = fread(magic, 1, sizeof(magic), f);
    if (bytes < sizeof(magic)) {
        fclose(f);
        return false;
    }

    int err = memcmp(magic, header_magic, sizeof(magic));
    if (err) {
        return false;
    }

    return true;
}
