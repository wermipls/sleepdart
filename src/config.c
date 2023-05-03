#include "config.h"
#include <stdlib.h>
#include "file.h"
#include "log.h"

static struct CfgField fields[] = {
    {"window-scale",    CFG_INT, NULL },
    {"limit-fps",       CFG_INT, NULL },
    {"palette",         CFG_STR, NULL },
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
