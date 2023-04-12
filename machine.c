#include "machine.h"

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
    memory_load_rom_16k(&machine->memory, "./rom/48.rom");

    machine->cpu.ctx = machine;
    cpu_init(&machine->cpu);

    return 0;
}

void machine_reset() 
{
    if (current_machine == NULL) return;
    current_machine->reset_pending = true;
}
