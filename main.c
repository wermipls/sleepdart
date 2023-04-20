#include <stdio.h>
#include "log.h"
#include "machine.h"
#include "ula.h"
#include "video_sdl.h"
#include "input_sdl.h"
#include "sound_sdl.h"
#include "tape.h"
#include "io.h"
#include "file.h"
#include "szx.h"
#include "palette.h"
#include "argparser.h"
#include "sleepdart_info.h"

int main(int argc, char *argv[])
{
    printf(".: sleepdart III THE FINAL :.\n");

    if (argc == 0) {
        printf("¿como estas?");
        return 0;
    }

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

    Machine_t m;

    machine_init(&m, MACHINE_ZX48K);
    machine_set_current(&m);

    Tape_t *tape = NULL;
    TapePlayer_t *player = NULL;
    char *file = argparser_get(parser, "file");

    if (file) {
        enum FileType ft = file_detect_type(file);

        if (ft == FTYPE_UNKNOWN) {
            dlog(LOG_ERR, "Unrecognized input file \"%s\"", file);
        }

        if (ft == FTYPE_TAP) {
            tape = tape_load_from_tap(file);
            player = tape_player_from_tape(tape);
            tape_player_pause(player, true);
            m.tape_player = player;
        }

        if (ft == FTYPE_SZX) {
            SZX_t *szx = szx_load_file(file);
            szx_state_load(szx, &m);
            szx_free(szx);
        }
    }

    ay_init(&m.ay, &m, 44100, 1750000);
    sound_sdl_init(44100);

    while (!m.cpu.error) {
        if (m.cpu.cycles < m.timing.t_int_hold) {
            cpu_fire_interrupt(&m.cpu);
        }
        cpu_do_cycles(&m.cpu);

        if (m.cpu.cycles >= m.timing.t_frame) {
            m.cpu.cycles -= m.timing.t_frame;
            m.frames++;

            if (palette_has_changed()) {
                Palette_t *pal = palette_load_current();
                if (pal) {
                    ula_set_palette(pal);
                    palette_free(pal);
                }
            }

            ula_naive_draw(&m);

            int quit = video_sdl_draw_rgb24_buffer(ula_buffer, sizeof(ula_buffer));
            if (quit) break;

            ay_process_frame(&m.ay);
            sound_sdl_queue(m.ay.buf, m.ay.buf_len * sizeof(float));

            input_sdl_update();

            if (player && input_sdl_get_key(SDL_SCANCODE_INSERT)) {
                tape_player_pause(player, false);
            }

            if (m.reset_pending) {
                cpu_init(&m.cpu);
                m.reset_pending = false;
            }

        }
    }

    ay_deinit(&m.ay);

    tape_player_close(player);
    tape_free(tape);

    palette_list_free();

    argparser_free(parser);
}
