#include <stdio.h>
#include "log.h"
#include "machine.h"
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

int main(int argc, char *argv[])
{
    ArgParser_t *parser = argparser_create(SLEEPDART_NAME);
    argparser_add_arg(parser, "file", 0, 0, true, "tape or snapshot file to be loaded");
    argparser_add_arg(parser, "--scale", 's', ARG_INT, 0, "integer window scale");
    argparser_add_arg(parser, "--fullscreen", 'f', ARG_STORE_TRUE, 0, "run in fullscreen mode");

    if (argparser_parse(parser, argc, argv)) {
        return -1;
    }

    palette_list_init();
    palette_set_default();

    int *scale = argparser_get(parser, "scale");

    int err = video_sdl_init(
        "third (sixth) iteration of sleepdart, the",
        BUFFER_WIDTH, BUFFER_HEIGHT, 
        scale ? *scale : 3);

    if (err) {
        dlog(LOG_ERR, "Failed to initialize video backend!");
        return 1;
    }

    if (argparser_get(parser, "fullscreen")) {
        video_sdl_toggle_window_mode();
    }

    input_sdl_init();

    Machine_t m = { 0 };

    machine_init(&m, MACHINE_ZX48K);
    machine_set_current(&m);

    video_sdl_set_fps((double)m.timing.clock_hz / (double)m.timing.t_frame);

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

    palette_list_free();

    argparser_free(parser);
}
