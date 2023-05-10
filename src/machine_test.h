#pragma once

struct Machine;

int machine_test_open(const char *path);
int machine_test_iterate(struct Machine *m);
void machine_test_close();
