#include <stdio.h>
#include <stdlib.h>
#include "log.h"
#include "machine.h"
#include "machine_test.h"
#include "ula.h"
#include "video_sdl.h"
#include "input_sdl.h"
#include "audio_sdl.h"
#include "tape.h"
#include "io.h"
#include "file.h"
#include "szx.h"
#include "palette.h"
#include "argparser.h"
#include "sleepdart_info.h"
#include "config.h"

int main(int argc, char *argv[])
{
    char *errsilent = getenv("SLEEPDART_ERRSILENT");
    if (errsilent) {
        log_force_errsilent();
    }

    ArgParser_t *parser = argparser_create(SLEEPDART_NAME);
    argparser_add_arg(parser, "file", 0, 0, true, "tape or snapshot file to be loaded");
    argparser_add_arg(parser, "--scale", 's', ARG_INT, 0, "integer window scale");
    argparser_add_arg(parser, "--fullscreen", 'f', ARG_STORE_TRUE, 0, "run in fullscreen mode");
    argparser_add_arg(parser, "--test", 0, ARG_STRING, 0, "perform an automated regression test");

    dlog(LOG_INFO, 
        SLEEPDART_NAME " version " SLEEPDART_VERSION ", built on " __DATE__ "\n");

    if (argparser_parse(parser, argc, argv)) {
        return -1;
    }

    config_init();

    palette_list_init();

    char *palette = config_get_str(&g_config, "palette");
    if (palette) {
        palette_set_by_name(palette);
        free(palette);
    } else {
        palette_set_default();
    }

    int *p_scale = argparser_get(parser, "scale");
    int scale = 2;
    if (p_scale) {
        scale = *p_scale;
    } else {
        config_get_int(&g_config, "window-scale", &scale);
    }

    int err = video_sdl_init(
        "third (sixth) iteration of sleepdart, the",
        BUFFER_WIDTH, BUFFER_HEIGHT, 
        scale);

    if (err) {
        dlog(LOG_ERR, "Failed to initialize video backend!");
        return 1;
    }

    if (argparser_get(parser, "fullscreen")) {
        video_sdl_toggle_window_mode();
    }
    
    int limit_fps;
    if (config_get_int(&g_config, "limit-fps", &limit_fps) == 0) {
        video_sdl_set_fps_limit(limit_fps);
    }

    input_sdl_init();

    Machine_t m = { 0 };

    machine_init(&m, MACHINE_ZX48K);
    machine_set_current(&m);

    video_sdl_set_fps((double)m.timing.clock_hz / (double)m.timing.t_frame);

    char *testpath = argparser_get(parser, "test");
    if (testpath) {
        int err = machine_test_open(testpath);
        if (err) {
            dlog(LOG_ERRSILENT, "Failed to run test!");
            machine_test_close();
            return 2;
        }
    }

    char *file = argparser_get(parser, "file");

    if (file) {
        machine_open_file(file);
    }

    int sample_rate = 44100;

    m.ay = ay_init(&m, sample_rate, 1750000);
    beeper_init(&m.beeper, &m, sample_rate);
    audio_sdl_init(sample_rate);

    machine_process_events();

    for (;;) {
        int err = machine_do_cycles();
        if (err) break;
    }

    ay_deinit(m.ay);

    config_set_int(&g_config, "window-scale", video_sdl_get_scale());
    config_set_int(&g_config, "limit-fps", video_sdl_get_fps_limit());
    config_set_str(&g_config, "palette", palette_get_name());

    config_save();

    argparser_free(parser);

    machine_test_close();
}
