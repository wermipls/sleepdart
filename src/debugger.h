#pragma once

#include <stdint.h>

struct Machine;

void debugger_handle();
void debugger_update_window();
void debugger_mark_dirty(uint16_t addr);
void debugger_open(struct Machine *m);
void debugger_close();
