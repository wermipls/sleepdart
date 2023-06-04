#include "log.h"

#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#if defined(_WIN32) && defined(PLATFORM_WIN32)
    #include <Windows.h>
    #include <stdlib.h>
    #include "unicode.h"
#endif

static const char prefix_err[] = "err: ";
static const char prefix_warn[] = "warn: ";
static const char prefix_none[] = "";
static int force_errsilent = 0;

void log_force_errsilent()
{
    force_errsilent = 1;
}

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
    if (l == LOG_ERR && !force_errsilent) {
        wchar_t *str = utf8_to_utf16(msg, NULL);
        if (str == NULL) {
            return;
        }
        MessageBoxW(GetActiveWindow(), str, L"Error", MB_OK | MB_ICONERROR);
        free(str);
    }
#endif
}
