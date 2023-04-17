#include "machine.h"
#include "file.h"

Machine_t *current_machine = NULL;

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

    memory_init(&machine->memory);

    char path[2048];
    file_path_append(path, file_get_basedir(), "rom/48.rom", sizeof(path));
    memory_load_rom_16k(&machine->memory, path);

    machine->cpu.ctx = machine;
    cpu_init(&machine->cpu);

    machine->tape_player = NULL;
    machine->reset_pending = false;

    return 0;
}

void machine_reset() 
{
    if (current_machine == NULL) return;
    current_machine->reset_pending = true;
}
