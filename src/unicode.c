#include "unicode.h"

static size_t strlen_u16(const uint16_t *str)
{
    if (!str) return 0;

    size_t len = 0;
    while (*str) {
        str++;
        len++;
    }

    return len;
}

uint16_t *utf8_to_utf16(const char *utf8)
{
    return (uint16_t *)SDL_iconv_string("UTF-16LE", "UTF-8", utf8, SDL_strlen(utf8) + 1);
}

char *utf16_to_utf8(const uint16_t *utf16)
{
    return SDL_iconv_string("UTF-8", "UTF-16LE", (const char *)utf16, (strlen_u16(utf16) + 1) * sizeof(uint16_t));
}
