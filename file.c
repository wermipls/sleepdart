#include "file.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>
#include <SDL2/SDL_filesystem.h>
#include "szx_file.h"

static char basedir[4096] = { 0 };

int64_t file_get_size(const char *path)
{
    if (path == NULL) return -1;

    struct stat s;
    if (stat(path, &s)) {
        return -2;
    }

    return s.st_size;
}

int file_is_regular_file(char *path)
{
    if (path == NULL) return -1;

    struct stat s;
    if (stat(path, &s)) {
        return -2;
    }

    if (S_ISREG(s.st_mode)) {
        return true;
    } else {
        return false;
    }
}

bool file_is_directory_separator(char c)
{
#ifdef _WIN32
    if (c == '/' || c == '\\') return true;
#else
    if (c == '/') return true;
#endif
    return false;
}

/* returns a pointer to the file extension in the path (w/o the dot),
 * or NULL if there is no valid extension present or on an error */
char *file_get_extension(char *path)
{
    if (path == NULL) {
        return NULL;
    }

    if (!file_is_regular_file(path)) {
        return NULL;
    }

    char *p = path;
    while (*p != 0) {
        p++;
    }

    p--;
    char *last = p;

    while (*p != '.') {
        if (file_is_directory_separator(*p)) {
            return NULL;
        }

        p--;

        if (p <= path || *p == 0) {
            return NULL;
        }
    }

    // no sane file ends with a dot.
    // windows makes it very difficult to create such files.
    // but let's handle it anyway just in case
    if (p >= last) return NULL;

    return p + 1;
}

const char *file_get_basedir()
{
    if (basedir[0]) {
        return basedir;
    }

    char *dir = SDL_GetBasePath();
    if (!dir) {
        return NULL;
    }
    strncpy(basedir, dir, sizeof(basedir)-1);
    SDL_free(dir);
    basedir[sizeof(basedir)-1] = 0;

    return basedir;
}

int file_path_append(char *dst, const char *a, const char *b, size_t len)
{
    if (!dst || !a || !b || !len) {
        return -1;
    }

    // FIXME: should use PathAppend on win32, probably?
    snprintf(dst, len, "%s%s", a, b);

#ifdef _WIN32
    char *p = dst;
    while (*p != 0) {
        if (*p == '/') {
            *p = '\\';
        }
        p++;
    }
#endif
    return 0;
}

enum FileType file_detect_type(char *path)
{
    char *ext = file_get_extension(path);

    if (ext != NULL) {
        // detect by extension
        size_t len = strlen(ext);
        char ext_lower[len+1];
        for (size_t i = 0; i < len; i++) {
            ext_lower[i] = tolower(ext[i]);
        }

        // .tap files aren't reliably possible to detect by binary data alone
        if (strncmp(ext_lower, "tap", 3) == 0) {
            return FILE_TAP;
        }
    }

    if (szx_is_valid_file(path)) {
        return FILE_SZX;
    }

    return FILE_UNKNOWN;
}
