#pragma once

/* returns a pointer to parsed value on success, NULL on fail.
 * user is expected to free the resulting pointer */
int *parse_int(char *str);

/* returns a pointer to parsed value on success, NULL on fail.
 * user is expected to free the resulting pointer */
float *parse_float(char *str);
