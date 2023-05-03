#include "config_parser.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <float.h>
#include <SDL2/SDL_assert.h>
#include "log.h"
#include "parser_helpers.h"

static char *process_string(FILE *f, int start, int end)
{
    int first = -1;
    int last = -1;
    int pos_old = ftell(f);

    if (start == end) {
        return NULL;
    }

    fseek(f, start, SEEK_SET);

    int c;
    while ((c = fgetc(f)) != EOF && start < end) {
        if (c != ' ') {
            last = ftell(f);
            if (first < 0) first = last;
        }

        start++;
    }

    if (first < 0) {
        fseek(f, pos_old, SEEK_SET);
        return NULL;
    }

    size_t len = last - first + 2;
    char *str = malloc(len);
    if (str == NULL) {
        fseek(f, pos_old, SEEK_SET);
        return NULL;
    }

    fseek(f, first-1, SEEK_SET);
    fread(str, 1, len-1, f);
    str[len-1] = 0;

    fseek(f, pos_old, SEEK_SET);
    return str;
}

static int config_find_index(CfgData_t *cfg, const char *key)
{
    for (size_t i = 0; i < cfg->len; i++) {
        int result = strcmp(cfg->data[i].key, key);
        if (result == 0) return i;
    }

    return -1;
}

static void config_process_keyvalue(
    CfgData_t *cfg, FILE *f, int start, int delimiter, int end, int line)
{
    char *key = process_string(f, start, delimiter);
    if (key == NULL) {
        dlog(LOG_WARN, "%s: invalid key on line %d", __func__, line);
        return;
    }

    char *value = process_string(f, delimiter+1, end-1);
    if (value == NULL) {
        dlog(LOG_WARN, "%s: invalid value on line %d", __func__, line);
        free(key);
        return;
    }

    int i = config_find_index(cfg, key);
    if (i < 0) {
        dlog(LOG_WARN, "%s: unknown key \"%s\" on line %d, ignoring", __func__, key, line);
        goto keyval_cleanup;
    }

    struct CfgField *e = &cfg->data[i];
    switch (e->type)
    {
    case CFG_STR:
        if (e->value) free(e->value);
        e->value = value;
        free(key);
        return;
    case CFG_INT: ;
        int *val_i = parse_int(value);
        if (val_i == NULL) goto keyval_cleanup;
        if (e->value) free(e->value);
        e->value = val_i;
        break;
    case CFG_FLOAT: ;
        float *val_f = parse_float(value);
        if (val_f == NULL) goto keyval_cleanup;
        if (e->value) free(e->value);
        e->value = val_f;
        break;
    }

keyval_cleanup:
    free(key);
    free(value);
}

int config_load_file(CfgData_t *cfg, char *path)
{
    FILE *f = fopen(path, "r");
    if (f == NULL) {
        return -1;
    }

    int line_no = 1;
    int line_start = 0;
    int key_end = -1;
    int value_end = -1;
    int line_ignore = 0;
    int c;
    while ((c = fgetc(f)) != EOF) {
        if (c == '\n') {
            if (key_end > 0) {
                if (value_end < 0) {
                    value_end = ftell(f)-1;
                }
                config_process_keyvalue(cfg, f, line_start, key_end, value_end, line_no);
            }
            line_start = ftell(f);
            key_end = -1;
            value_end = -1;
            line_ignore = 0;
            line_no++;
            continue;
        }

        if (line_ignore) {
            continue;
        }

        if (c == '#') {
            value_end = ftell(f)-1;
            line_ignore = 1;
        }

        if (c == '=' && key_end < 0) {
            key_end = ftell(f)-1;
        }
    }

    if (key_end > 0) {
        if (value_end < 0) {
            value_end = ftell(f)+1;
        }
        config_process_keyvalue(cfg, f, line_start, key_end, value_end, line_no);
    }

    fclose(f);
    return 0;
}

int config_save_file(CfgData_t *cfg, char *path)
{
    FILE *f = fopen(path, "w");
    if (f == NULL) {
        return -1;
    }

    fputs("# Auto-generated file. Contents may get overwritten.\n\n", f);

    for (size_t i = 0; i < cfg->len; i++) {
        struct CfgField *e = &cfg->data[i];
        if (e->value == NULL) {
            continue;
        }

        switch (e->type)
        {
        case CFG_STR:
            fprintf(f, "%s = %s\n", e->key, (char *)e->value);
            break;
        case CFG_INT:
            fprintf(f, "%s = %d\n", e->key, *(int *)e->value);
            break;
        case CFG_FLOAT:
            fprintf(f, "%s = %.*g\n", e->key, FLT_DECIMAL_DIG, *(float *)e->value);
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
    if (e->value == NULL) {
        return NULL;
    }

    SDL_assert_release(e->type == CFG_STR);

    size_t len = strlen(e->value) + 1;

    char *str = malloc(len);
    if (str == NULL) {
        return NULL;
    }

    memcpy(str, e->value, len);

    return str;
}

int config_get_int(CfgData_t *cfg, const char *key, int *dest)
{
    int i = config_find_index(cfg, key);
    if (i < 0) {
        return -1;
    }

    struct CfgField *e = &cfg->data[i];
    if (e->value == NULL) {
        return -2;
    }

    SDL_assert_release(e->type == CFG_INT);

    *dest = *(int *)e->value;
    return 0;
}

int config_get_float(CfgData_t *cfg, const char *key, float *dest)
{
    int i = config_find_index(cfg, key);
    if (i < 0) {
        return -1;
    }

    struct CfgField *e = &cfg->data[i];
    if (e->value == NULL) {
        return -2;
    }

    SDL_assert_release(e->type == CFG_FLOAT);

    *dest = *(float *)e->value;
    return 0;
}

void config_set_str(CfgData_t *cfg, const char *key, char *value)
{
    if (value == NULL) {
        return;
    }

    int i = config_find_index(cfg, key);
    if (i < 0) {
        return;
    }


    size_t len = strlen(value) + 1;

    struct CfgField *e = &cfg->data[i];
    SDL_assert_release(e->type == CFG_STR);

    if (e->value != NULL) {
        free(e->value);
    }

    e->value = malloc(len);
    if (e->value == NULL) {
        return;
    }

    memcpy(e->value, value, len);
}

void config_set_int(CfgData_t *cfg, const char *key, int value)
{
    int i = config_find_index(cfg, key);
    if (i < 0) {
        return;
    }

    struct CfgField *e = &cfg->data[i];
    SDL_assert_release(e->type == CFG_INT);

    if (e->value == NULL) {
        e->value = malloc(sizeof(int));
        if (e->value == NULL) {
            return;
        }
    }

    *(int *)e->value = value;
}

void config_set_float(CfgData_t *cfg, const char *key, float value)
{
    int i = config_find_index(cfg, key);
    if (i < 0) {
        return;
    }

    struct CfgField *e = &cfg->data[i];
    SDL_assert_release(e->type == CFG_FLOAT);

    if (e->value == NULL) {
        e->value = malloc(sizeof(float));
        if (e->value == NULL) {
            return;
        }
    }

    *(float *)e->value = value;
}
