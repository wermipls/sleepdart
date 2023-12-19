#include "config.h"
#include <stdlib.h>
#include "file.h"
#include "log.h"

static struct CfgField fields[] = {
    {"window-scale",        CFG_INT, NULL },
    {"limit-fps",           CFG_INT, NULL },
    {"palette",             CFG_STR, NULL },
    {"ay-pan-a",            CFG_FLOAT, NULL },
    {"ay-pan-b",            CFG_FLOAT, NULL },
    {"ay-pan-c",            CFG_FLOAT, NULL },
    {"ay-pan-equal-power",  CFG_INT, NULL },
    {"ay-high-quality",     CFG_INT, NULL },
};

CfgData_t g_config = {
    .data = fields,
    .len = sizeof(fields) / sizeof(struct CfgField),
};

static char config_path[2048] = { 0 };

void config_defaults()
{
    config_set_int(&g_config, "window-scale", 2);
    config_set_int(&g_config, "limit-fps", 1);
    config_set_str(&g_config, "palette", "loni");
    config_set_float(&g_config, "ay-pan-a", 0.25);
    config_set_float(&g_config, "ay-pan-b", 0.5);
    config_set_float(&g_config, "ay-pan-c", 0.75);
    config_set_int(&g_config, "ay-pan-equal-power", 1);
    config_set_int(&g_config, "ay-high-quality", 0);
}

void config_init()
{
    config_defaults();

    file_path_append(
        config_path, file_get_basedir(), "config.ini", sizeof(config_path));

    config_load_file(&g_config, config_path);
}

void config_save()
{
    int err = config_save_file(&g_config, config_path);
    if (err) {
        dlog(LOG_ERR, "failed to save config!");
    }
}
