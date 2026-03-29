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
int file_path_append(char *dst, const char *a, const char *b, size_t len);
void file_free_list(char **list);
char **file_list_directory_files(char *path);
enum FileType file_detect_type(char *path);
char *file_read_line(FILE *f);

#ifdef _WIN32
    FILE *fopen_utf8_win32(const char *path, const wchar_t *mode);
    #define fopen_utf8(path, mode) fopen_utf8_win32(path, L ## mode)
#else
    #define fopen_utf8(path, mode) fopen(path, mode)
#endif
