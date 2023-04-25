#include "machine.h"
#include "file.h"

Machine_t *current_machine = NULL;

const struct MachineTiming machine_timing_zx48k = {
    .clock_hz = 3500000,

    .t_firstpx = 14336,
    .t_scanline = 224,
    .t_screen = 128,
    .t_frame = 224 * 312,
    .t_eightpx = 4,
    .t_int_hold = 32,
};

void machine_set_current(Machine_t *machine)
{
    current_machine = machine;
}

int machine_init(Machine_t *machine, enum MachineType type)
{
    if (machine == NULL) {
        return -1;
    }

    if (type != MACHINE_ZX48K) {
        return -2;
    }

    machine->timing = machine_timing_zx48k;

    memory_init(&machine->memory);

    char path[2048];
    file_path_append(path, file_get_basedir(), "rom/48.rom", sizeof(path));
    memory_load_rom_16k(&machine->memory, path);

    machine->cpu.ctx = machine;
    cpu_init(&machine->cpu);

    machine->tape_player = NULL;
    machine->frames = 0;
    machine->reset_pending = false;

    // FIXME: hack
    ula_init(machine);

    return 0;
}

void machine_reset() 
{
    if (current_machine == NULL) return;
    current_machine->reset_pending = true;
}
