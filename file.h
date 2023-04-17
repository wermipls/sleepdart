#pragma once

#include <stdbool.h>
#include <stdint.h>

enum FileType {
    FILE_UNKNOWN = -1,
    FILE_TAP,
    FILE_SZX,
};

int64_t file_get_size(const char *path);
const char *file_get_basedir();
int file_path_append(char *dst, const char *a, const char *b, size_t len);
enum FileType file_detect_type(char *path);
