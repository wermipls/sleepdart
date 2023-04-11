#include <stdio.h>
#include "log.h"
#include "z80.h"
#include "memory.h"
#include "ula.h"
#include "video_sdl.h"
#include "input_sdl.h"
#include "tape.h"
#include "io.h"

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

    memory_init();
    memory_load_rom_16k("./rom/48.rom");

    Tape_t *tape = NULL;
    TapePlayer_t *player = NULL;

    if (argc > 1) {
        tape = tape_load_from_tap(argv[1]);
        player = tape_player_from_tape(tape);
        tape_player_pause(player, true);

        io_set_tape_player(player);
    }

    cpu_init(&cpu);

    int cycles = -1;
    int frame = 0;
    while (cycles) {
        cycles = cpu_do_cycles(&cpu);
        // FIXME: HACK
        if (cpu.cycles > T_FRAME) {
            cpu.cycles -= T_FRAME;

            ula_naive_draw();
            cpu_fire_interrupt(&cpu);

            int quit = video_sdl_draw_rgb24_buffer(ula_buffer, sizeof(ula_buffer));
            if (quit) break;

            input_sdl_update();

            if (tape && input_sdl_get_key(SDL_SCANCODE_INSERT)) {
                tape_player_pause(player, false);
            } 

            tape_player_advance_cycles(player, T_FRAME - last_tape_read);
            last_tape_read = 0;

            frame++;
        }
    }

    tape_player_close(player);
    tape_free(tape);
}
