#pragma once

#include <stdbool.h>
#include <stdint.h>

enum FileType {
    FTYPE_UNKNOWN = -1,
    FTYPE_TAP,
    FTYPE_SZX,
};

int64_t file_get_size(const char *path);
char *file_get_extension(char *path);
const char *file_get_basedir();
int file_path_append(char *dst, const char *a, const char *b, size_t len);
void file_free_list(char *list[]);
char **file_list_directory_files(char *path);
enum FileType file_detect_type(char *path);
