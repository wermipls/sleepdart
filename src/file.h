#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>

enum FileType {
    FTYPE_UNKNOWN = -1,
    FTYPE_TAP,
    FTYPE_SZX,
    FTYPE_SNA,
};

bool file_init(const char *argv0);
bool file_exists(const char *path);
int64_t file_get_size(const char *path);
char *file_get_extension(char *path);
const char *file_get_basedir();
const char *file_get_prefdir();

/* Joins `a` and `b` into a single path. Returns the path or NULL on error.
 * Returned path is allocated on the heap and must be freed with `free()`.
 * If `dst` is provided, it MUST point to memory allocated with `malloc()`.
 * `dst` and `a` can be the same pointer. */
char *file_path_append(char *dst, const char *a, const char *b);

enum FileType file_detect_type(char *path);
char *file_read_line(FILE *f);

#ifdef _WIN32
    FILE *fopen_utf8_win32(const char *path, const wchar_t *mode);
    #define fopen_utf8(path, mode) fopen_utf8_win32(path, L ## mode)
#else
    #define fopen_utf8(path, mode) fopen(path, mode)
#endif
