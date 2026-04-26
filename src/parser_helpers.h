#pragma once

#include <stdbool.h>

/* returns true on success. parsed value is written to dst. */
bool parse_int(const char *str, int *dst);

/* returns true on success. parsed value is written to dst. */
bool parse_float(const char *str, float *dst);
