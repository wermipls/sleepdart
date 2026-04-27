#pragma once

#include <stdbool.h>
#include <SDL3/SDL_iostream.h>
#include "machine.h"

bool sna_state_load_io(SDL_IOStream *io, Machine_t *m);
bool sna_state_load(char *path, Machine_t *m);
