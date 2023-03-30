#include <stdio.h>
#include "z80.h"
#include "memory.h"

int main()
{
    printf(".: sleepdart III THE FINAL :.\n");

    memory_init();
    memory_load_rom_16k("./rom/48.rom");

    cpu_init(&cpu);

    int cycles = -1;
    while (cycles) {
        cycles = cpu_do_cycles(&cpu);
    }
}
