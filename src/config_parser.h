#pragma once

#include <stddef.h>

enum CfgType
{
    CFG_STR,
    CFG_INT,
    CFG_FLOAT,
};

struct CfgField
{
    const char *key;
    enum CfgType type;
    void *value;
};

typedef struct CfgData
{
    size_t len;
    struct CfgField *data;
} CfgData_t;

int config_load_file(CfgData_t *cfg, char *path);
int config_save_file(CfgData_t *cfg, char *path);

/* returns a copy of the string on success, NULL on fail.
 * needs to be freed by user */
char *config_get_str(CfgData_t *cfg, const char *key);

/* returns 0 on success, non-zero otherwise
 * config value gets copied to dest */
int config_get_int(CfgData_t *cfg, const char *key, int *dest);

/* returns 0 on success, non-zero otherwise
 * config value gets copied to dest */
int config_get_float(CfgData_t *cfg, const char *key, float *dest);

void config_set_str(CfgData_t *cfg, const char *key, const char *value);
void config_set_int(CfgData_t *cfg, const char *key, int value);
void config_set_float(CfgData_t *cfg, const char *key, float value);
