#include "sna.h"
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <inttypes.h>
#include <stdio.h>
#include "file.h"
#include "log.h"
#include "ula.h"

struct SNA
{
    uint8_t i;
    uint16_t hl_, de_, bc_, af_;
    uint16_t hl, de, bc, iy, ix;
    uint8_t interrupt;
    uint8_t r;
    uint16_t af, sp;
    uint8_t im;
    uint8_t border;
    uint8_t ram[1];
} __attribute__((packed));

#define SNA_IFF2 (1<<2)
#define SNA_SIZE_48K 49179

#define ALIGN_TO(size, align) (((size+align-1) / align) * align)

int sna_state_load(char *path, Machine_t *m)
{
    // .sna is yet another format that you can't quite distinguish
    // by its contents alone. A 48K snapshot is typically 
    // 49179 bytes though, sometimes 49280 due to padding
    // (seems +3DOS in particular rounds up the sizes to 128 bytes).
    int64_t file_size = file_get_size(path);
    if (file_size < SNA_SIZE_48K && file_size > ALIGN_TO(SNA_SIZE_48K, 128)) {
        dlog(LOG_ERRSILENT, "File does not appear to be a valid 48K snapshot");
        return -1;
    } else {
        file_size = SNA_SIZE_48K;
    }

    FILE *f = fopen(path, "rb");
    if (f == NULL) {
        dlog(LOG_ERRSILENT, "Failed to open file for reading");
        return -1;
    }

    struct SNA *sna = malloc(file_size);
    if (sna == NULL) {
        dlog(LOG_ERRSILENT, "Failed to allocate memory for the snapshot");
        fclose(f);
        return -2;
    }

    size_t bytes = fread(sna, 1, file_size, f);
    if (bytes != file_size) {
        dlog(LOG_ERRSILENT, "Read %zu bytes, expected %"PRId64, bytes, file_size);
        free(sna);
        fclose(f);
        return -3;
    }

    machine_deinit(m);
    machine_init(m, MACHINE_ZX48K);
    ay_reset(m->ay);

    memcpy(m->memory.bus+0x4000, sna->ram, 0xC000);
    ula_reset_screen_dirty();
    ula_set_border(sna->border & 7, 0);

    m->cpu.regs.main.af = sna->af;
    m->cpu.regs.main.bc = sna->bc;
    m->cpu.regs.main.de = sna->de;
    m->cpu.regs.main.hl = sna->hl;

    m->cpu.regs.alt.af = sna->af_;
    m->cpu.regs.alt.bc = sna->bc_;
    m->cpu.regs.alt.de = sna->de_;
    m->cpu.regs.alt.hl = sna->hl_;

    m->cpu.regs.ix = sna->ix;
    m->cpu.regs.iy = sna->iy;

    m->cpu.regs.sp = sna->sp;
    m->cpu.regs.i = sna->i;
    m->cpu.regs.r = sna->r;
    m->cpu.regs.im = sna->im;
    m->cpu.regs.iff1 = sna->interrupt & SNA_IFF2;
    m->cpu.regs.iff2 = sna->interrupt & SNA_IFF2;

    // Note that 48K snapshot does not store the PC anywhere directly,
    // and typically reti is required to start the execution.
    // Instead, I'll just push the stack around manually.
    uint8_t l = memory_bus_peek(m->memory.bus, m->cpu.regs.sp++);
    uint8_t h = memory_bus_peek(m->memory.bus, m->cpu.regs.sp++);
    m->cpu.regs.pc = (h << 8) | l;

    free(sna);
    fclose(f);
    return 0;
}
