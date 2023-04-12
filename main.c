#include <stdio.h>
#include "log.h"
#include "machine.h"
#include "ula.h"
#include "video_sdl.h"
#include "input_sdl.h"
#include "tape.h"
#include "io.h"
#include "file.h"
#include "szx.h"

int main(int argc, char *argv[])
{
    printf(".: sleepdart III THE FINAL :.\n");

    if (argc == 0) {
        printf("¿como estas?");
        return 0;
    }

    int err = video_sdl_init(
        "third (sixth) iteration of sleepdart, the",
        BUFFER_WIDTH, BUFFER_HEIGHT, 
        3); // scale

    if (err) {
        dlog(LOG_ERR, "Failed to initialize video backend!");
        return 1;
    }

    input_sdl_init();

    Machine_t m;

    machine_init(&m, MACHINE_ZX48K);
    machine_set_current(&m);

    Tape_t *tape = NULL;
    TapePlayer_t *player = NULL;

    if (argc > 1) {
        enum FileType ft = file_detect_type(argv[1]);

        if (ft == FILE_UNKNOWN) {
            dlog(LOG_ERR, "Unrecognized input file \"%s\"", argv[1]);
        }

        if (ft == FILE_TAP) {
            tape = tape_load_from_tap(argv[1]);
            player = tape_player_from_tape(tape);
            tape_player_pause(player, true);

            io_set_tape_player(player);
        }

        if (ft == FILE_SZX) {
            SZX_t *szx = szx_load_file(argv[1]);
            szx_state_load(szx, &m);
            szx_free(szx);
        }
    }

    int cycles = -1;
    int frame = 0;
    while (cycles) {
        cycles = cpu_do_cycles(&m.cpu);
        // FIXME: HACK
        if (m.cpu.cycles > T_FRAME) {
            m.cpu.cycles -= T_FRAME;

            ula_naive_draw(&m.memory);
            cpu_fire_interrupt(&m.cpu);

            int quit = video_sdl_draw_rgb24_buffer(ula_buffer, sizeof(ula_buffer));
            if (quit) break;

            input_sdl_update();

            if (tape && input_sdl_get_key(SDL_SCANCODE_INSERT)) {
                tape_player_pause(player, false);
            } 

            tape_player_advance_cycles(player, T_FRAME - last_tape_read);
            last_tape_read = 0;

            if (m.reset_pending) {
                cpu_init(&m.cpu);
                m.reset_pending = false;
            }

            frame++;
        }
    }

    tape_player_close(player);
    tape_free(tape);
}
