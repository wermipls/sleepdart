// Code from https://github.com/Photosounder/rouziclib
// See unicode.c for license details

#pragma once

#include <stddef.h>
#include <stdint.h>

extern int utf8_char_size(const char *c);
extern int codepoint_utf8_size(const uint32_t c);
extern uint32_t utf8_to_unicode32(const char *c, size_t *index);
extern char *sprint_unicode(char *str, uint32_t c);
extern int find_prev_utf8_char(const char *str, size_t pos);
extern int find_next_utf8_char(const char *str, size_t pos);

extern size_t strlen_utf16(const uint16_t *str);
extern int utf16_char_size(const uint16_t *c);
extern int codepoint_utf16_size(uint32_t c);
extern uint32_t utf16_to_unicode32(const uint16_t *c, size_t *index);
extern uint16_t *sprint_utf16(uint16_t *str, uint32_t c);
extern size_t strlen_utf8_to_utf16(const char *str);
extern size_t strlen_utf16_to_utf8(const uint16_t *str);
extern uint16_t *utf8_to_utf16(const char *utf8, uint16_t *utf16);
extern char *utf16_to_utf8(const uint16_t *utf16, char *utf8);

#define utf8_to_wchar(utf8, wchar)	utf8_to_utf16(utf8, wchar)
#define wchar_to_utf8(wchar, utf8)	utf16_to_utf8(wchar, utf8)
