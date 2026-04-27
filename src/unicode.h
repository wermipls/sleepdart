#pragma once

#include <stddef.h>
#include <stdint.h>
#include <SDL3/SDL_stdinc.h>

/* must be freed with SDL_free() */
uint16_t *utf8_to_utf16(const char *utf8);

/* must be freed with SDL_free() */
char *utf16_to_utf8(const uint16_t *utf16);
