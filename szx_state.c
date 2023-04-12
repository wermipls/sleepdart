#include "szx_state.h"
#include "szx_blocks.h"
#include "assert.h"

#define U32_FROM_CH(a,b,c,d) ((a) | ((b)<<8) | ((c)<<16) | ((d)<<24))

void szx_load_block_z80regs(struct SZXBlock *b, Machine_t *m)
{
    SZXZ80Regs_t *r = (SZXZ80Regs_t *)b->data;
    m->cpu.regs.main.af = r->af;
    m->cpu.regs.main.bc = r->bc;
    m->cpu.regs.main.de = r->de;
    m->cpu.regs.main.hl = r->hl;

    m->cpu.regs.alt.af = r->af1;
    m->cpu.regs.alt.bc = r->bc1;
    m->cpu.regs.alt.de = r->de1;
    m->cpu.regs.alt.hl = r->hl1;

    m->cpu.regs.ix = r->ix;
    m->cpu.regs.iy = r->iy;
    m->cpu.regs.sp = r->sp;
    m->cpu.regs.pc = r->pc;

    m->cpu.regs.i = r->i;
    m->cpu.regs.r = r->r;

    m->cpu.regs.iff1 = r->iff1;
    m->cpu.regs.iff2 = r->iff2;

    m->cpu.cycles = r->cycles_start;
}

int szx_state_load(SZX_t *szx, struct Machine *m)
{
    if (szx->header.machine_id != SZX_MID_48K) {
        return -1;
    }

    machine_init(m, MACHINE_ZX48K);

    for (size_t i = 0; i < szx->blocks; i++) {
        struct SZXBlock *block = &szx->block[i];

        switch (block->header.id)
        {
        case U32_FROM_CH('Z','8','0','R'):
            szx_load_block_z80regs(block, m);
            break;
        }
    }

    return 0;
}
