#include "config_parser.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <float.h>
#include <SDL3/SDL_assert.h>
#include <limits.h>
#include "log.h"
#include "parser_helpers.h"
#include "file.h"

static char *trim_whitespace(char *str, size_t len)
{
    char *p_first = NULL;
    char *p_last = NULL;

    while (*str != 0 && len) {
        if (*str != '\t' && *str != ' ') {
            p_last = str;
            if (!p_first) {
                p_first = str;
            }
        }
        str++;
        len--;
    }

    if (p_first == NULL) {
        return NULL;
    }

    size_t size = p_last - p_first + 2;
    char *new = malloc(size);
    if (new == NULL) {
        return NULL;
    }
    strncpy(new, p_first, size-1);
    new[size-1] = 0;
    return new;
}

static int config_find_index(CfgData_t *cfg, const char *key)
{
    for (size_t i = 0; i < cfg->len; i++) {
        int result = strcmp(cfg->data[i].key, key);
        if (result == 0) return i;
    }

    return -1;
}

static void config_process_keyvalue(CfgData_t *cfg, char *key, char *value, int line)
{
    int i = config_find_index(cfg, key);
    if (i < 0) {
        dlog(LOG_WARN, "%s: unknown key \"%s\" on line %d, ignoring", __func__, key, line);
    }

    struct CfgField *e = &cfg->data[i];
    switch (e->type)
    {
    case CFG_STR: ;
        size_t sz = strlen(value) + 1;
        char *v_new = realloc(e->value_str, sz);
        if (!v_new) {
            break;
        }
        memcpy(v_new, value, sz);
        e->value_str = v_new;
        e->has_value = true;
        break;
    case CFG_INT: ;
        if (!parse_int(value, &e->value_int)) {
            break;
        }
        e->has_value = true;
        break;
    case CFG_FLOAT: ;
        if (!parse_float(value, &e->value_float)) {
            break;
        }
        e->has_value = true;
        break;
    }
}

int config_load_file(CfgData_t *cfg, char *path)
{
    FILE *f = fopen_utf8(path, "r");
    if (f == NULL) {
        return -1;
    }

    char *line;
    int line_no = 0;
    while ((line = file_read_line(f)) != NULL) {
        line_no++;
        char *p_value = strchr(line, '=');
        char *p_comment = strchr(line, '#');
        if (p_value == NULL) {
            free(line);
            continue;
        }

        if (p_comment != NULL) {
            if (p_comment < p_value) {
                free(line);
                continue;
            }
        }

        size_t key_len = p_value - line;
        p_value++;

        char *key = trim_whitespace(line, key_len);
        if (key == NULL) {
            dlog(LOG_WARN, "%s: invalid key on line %d", __func__, line_no);
            free(line);
            continue;
        }

        size_t value_len = p_comment ? (size_t)(p_comment - p_value) : SIZE_MAX;
        char *value = trim_whitespace(p_value, value_len);
        if (value == NULL) {
            dlog(LOG_WARN, "%s: invalid value on line %d", __func__, line_no);
            free(key);
            free(line);
            continue;
        }

        config_process_keyvalue(cfg, key, value, line_no);
        free(key);
        free(value);
        free(line);
    }

    fclose(f);
    return 0;
}

int config_save_file(CfgData_t *cfg, char *path)
{
    FILE *f = fopen_utf8(path, "w");
    if (f == NULL) {
        return -1;
    }

    fputs("# Auto-generated file. Contents may get overwritten.\n\n", f);

    for (size_t i = 0; i < cfg->len; i++) {
        struct CfgField *e = &cfg->data[i];
        if (!e->has_value) {
            continue;
        }

        switch (e->type)
        {
        case CFG_STR:
            fprintf(f, "%s = %s\n", e->key, e->value_str);
            break;
        case CFG_INT:
            fprintf(f, "%s = %d\n", e->key, e->value_int);
            break;
        case CFG_FLOAT:
            fprintf(f, "%s = %.*g\n", e->key, FLT_DECIMAL_DIG, e->value_float);
            break;
        }
    }

    fclose(f);
    return 0;
}

char *config_get_str(CfgData_t *cfg, const char *key)
{
    int i = config_find_index(cfg, key);
    if (i < 0) {
        return NULL;
    }

    struct CfgField *e = &cfg->data[i];
    if (!e->has_value) {
        return NULL;
    }

    SDL_assert_release(e->type == CFG_STR);

    size_t sz = strlen(e->value_str) + 1;

    char *str = malloc(sz);
    if (str == NULL) {
        return NULL;
    }

    memcpy(str, e->value_str, sz);

    return str;
}

int config_get_int(CfgData_t *cfg, const char *key, int *dest)
{
    int i = config_find_index(cfg, key);
    if (i < 0) {
        return -1;
    }

    struct CfgField *e = &cfg->data[i];
    if (!e->has_value) {
        return -2;
    }

    SDL_assert_release(e->type == CFG_INT);

    *dest = e->value_int;
    return 0;
}

int config_get_float(CfgData_t *cfg, const char *key, float *dest)
{
    int i = config_find_index(cfg, key);
    if (i < 0) {
        return -1;
    }

    struct CfgField *e = &cfg->data[i];
    if (!e->has_value) {
        return -2;
    }

    SDL_assert_release(e->type == CFG_FLOAT);

    *dest = e->value_float;
    return 0;
}

void config_set_str(CfgData_t *cfg, const char *key, const char *value)
{
    if (value == NULL) {
        return;
    }

    int i = config_find_index(cfg, key);
    if (i < 0) {
        return;
    }


    size_t sz = strlen(value) + 1;

    struct CfgField *e = &cfg->data[i];
    SDL_assert_release(e->type == CFG_STR);

    if (e->has_value) {
        free(e->value_str);
    }

    e->value_str = malloc(sz);
    if (e->value_str == NULL) {
        e->has_value = false;
        return;
    }

    e->has_value = true;
    memcpy(e->value_str, value, sz);
}

void config_set_int(CfgData_t *cfg, const char *key, int value)
{
    int i = config_find_index(cfg, key);
    if (i < 0) {
        return;
    }

    struct CfgField *e = &cfg->data[i];
    SDL_assert_release(e->type == CFG_INT);

    e->value_int = value;
    e->has_value = true;
}

void config_set_float(CfgData_t *cfg, const char *key, float value)
{
    int i = config_find_index(cfg, key);
    if (i < 0) {
        return;
    }

    struct CfgField *e = &cfg->data[i];
    SDL_assert_release(e->type == CFG_FLOAT);

    e->value_float = value;
    e->has_value = true;
}
