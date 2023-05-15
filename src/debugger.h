#pragma once

struct Machine;

void debugger_handle();
void debugger_update_window();
void debugger_open(struct Machine *m);
void debugger_close();
