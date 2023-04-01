#include <stdio.h>
#include "log.h"
#include "z80.h"
#include "memory.h"
#include "ula.h"
#include "video_sdl.h"

int main()
{
    printf(".: sleepdart III THE FINAL :.\n");

    int err = video_sdl_init(
        "third (sixth) iteration of sleepdart, the",
        BUFFER_WIDTH, BUFFER_HEIGHT, 
        3); // scale

    if (err) {
        dlog(LOG_ERR, "Failed to initialize video backend!");
        return 1;
    }

    memory_init();
    memory_load_rom_16k("./rom/48.rom");

    cpu_init(&cpu);

    int cycles = -1;
    int frame = 0;
    while (cycles) {
        cycles = cpu_do_cycles(&cpu);
        // FIXME: HACK
        if (cpu.cycles > T_FRAME) {
            cpu.cycles -= T_FRAME;

            ula_naive_draw();

            video_sdl_draw_rgb24_buffer(ula_buffer, sizeof(ula_buffer));

            frame++;
        }
    }
}
