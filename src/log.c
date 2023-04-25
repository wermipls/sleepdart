#include "log.h"

#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#if defined(_WIN32) && defined(PLATFORM_WIN32)
    #include <Windows.h>
#endif

static const char prefix_err[] = "err: ";
static const char prefix_warn[] = "warn: ";
static const char prefix_none[] = "";

void dlog(enum LogLevel l, char fmt[], ...)
{
    const char *prefix;

    switch (l)
    {
    case LOG_ERR:
    case LOG_ERRSILENT:
        prefix = prefix_err;
        break;
    case LOG_WARN:
        prefix = prefix_warn;
        break;
    default:
        prefix = prefix_none;
        break;
    }

    va_list argv;
    va_start(argv, fmt);

    char msg[1024];
    vsnprintf(msg, 1024, fmt, argv);

    va_end(argv);

    // console output
    fprintf(stderr, "%s%s\n", prefix, msg);
    
#if defined(_WIN32) && defined(PLATFORM_WIN32)
    if (l == LOG_ERR) {
        MessageBoxA(GetActiveWindow(), msg, "Error", MB_OK | MB_ICONERROR);
    }
#endif
}
