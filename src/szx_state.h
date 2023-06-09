#pragma once

#include "szx_file.h"
#include "machine.h"

int szx_state_load(SZX_t *szx, Machine_t *m);
SZX_t *szx_state_save(struct Machine *m);
