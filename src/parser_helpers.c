#include "parser_helpers.h"
#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include "log.h"

bool parse_int(const char *str, int *dst)
{
    _Static_assert(sizeof(int) == sizeof(long), "int and long should be same size.");

    char *end;
    errno = 0;
    long value = strtol(str, &end, 0);
    if (end == str || (*end != 0 && *end != ' ')) {
        dlog(LOG_ERRSILENT, "Failed to parse \"%s\" as an integer", str);
        return false;
    }

    if (errno == ERANGE) {
        dlog(LOG_ERRSILENT, "Integer \"%s\" is out of range", str);
        return false;
    }

    *dst = value;
    return true;
}

bool parse_float(const char *str, float *dst)
{
    char *end;
    errno = 0;
    float value = strtof(str, &end);
    if (end == str || (*end != 0 && *end != ' ')) {
        dlog(LOG_ERRSILENT, "Failed to parse \"%s\" as a floating point value", str);
        return false;
    }

    if (errno == ERANGE) {
        dlog(LOG_ERRSILENT, "Float \"%s\" is out of range", str);
        return false;
    }

    *dst = value;
    return true;
}
