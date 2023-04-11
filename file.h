#pragma once

#include <stdbool.h>

enum FileType {
    FILE_UNKNOWN = -1,
    FILE_TAP,
    FILE_SZX,
};

enum FileType file_detect_type(char *path);
