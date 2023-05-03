#include "parser_helpers.h"
#include <errno.h>
#include <stdlib.h>
#include "log.h"

int *parse_int(char *str)
{
    char *end;
    long value = strtol(str, &end, 0);
    if (end == str || (*end != 0 && *end != ' ')) {
        dlog(LOG_ERRSILENT, "Failed to parse \"%s\" as an integer", str);
        return NULL;
    }

    if (errno == ERANGE) {
        dlog(LOG_ERRSILENT, "Integer \"%s\" is out of range", str);
        return NULL;
    }

    int *i = malloc(sizeof(i));
    if (!i) {
        dlog(LOG_ERRSILENT, "%s: malloc fail", __func__);
        return NULL;
    }

    // FIXME: long -> int cast
    // long and int are both same size on gcc x86_64-w64-mingw32 but yea...
    *i = value;
    return i;
}

float *parse_float(char *str)
{
    char *end;
    float value = strtof(str, &end);
    if (end == str || (*end != 0 && *end != ' ')) {
        dlog(LOG_ERRSILENT, "Failed to parse \"%s\" as a floating point value", str);
        return NULL;
    }

    if (errno == ERANGE) {
        dlog(LOG_ERRSILENT, "Float \"%s\" is out of range", str);
        return NULL;
    }

    float *i = malloc(sizeof(i));
    if (!i) {
        dlog(LOG_ERRSILENT, "%s: malloc fail", __func__);
        return NULL;
    }

    *i = value;
    return i;
}
