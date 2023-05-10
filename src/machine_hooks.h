#pragma once

#include <stdio.h>

struct Machine;

void machine_set_print_stream(FILE *f);
void machine_process_hooks(struct Machine *m);
