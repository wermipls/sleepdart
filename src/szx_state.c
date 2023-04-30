#include "szx_state.h"
#include "szx_blocks.h"
#include "ula.h"
#include "io.h"
#include <string.h>
#include <zlib.h>

#define U32_FROM_CH(a,b,c,d) ((a) | ((b)<<8) | ((c)<<16) | ((d)<<24))

int szx_load_block_rampage(struct SZXBlock *b, Machine_t *m)
{
    SZXRAMPage_t *page = (SZXRAMPage_t *)b->data;

    int is_compressed = page->flags & SZX_RF_COMPRESSED;

    uint8_t *dest;

    switch (page->page_no)
    {
    case 5:
        dest = &m->memory.bus[0x4000];
        break;
    case 2:
        dest = &m->memory.bus[0x8000];
        break;
    case 0:
        dest = &m->memory.bus[0xC000];
        break;
    default:
        return 0;
    }

    size_t size = b->header.size - (sizeof(SZXRAMPage_t) - 1);
    long unsigned int destlen = 0x4000;

    if (is_compressed) {
        int err = uncompress(dest, &destlen, page->data, size);
        if (err) return -1;
    } else {
        if (size != 0x4000) return -2;
        memcpy(dest, page->data, size);
    }

    return 0;
}

int szx_load_block_z80regs(struct SZXBlock *b, Machine_t *m)
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

    m->cpu.regs.im = r->im;

    m->cpu.cycles = r->cycles_start;
    m->cpu.halted = r->flags & SZX_ZF_HALTED;
    m->cpu.last_ei = r->flags & SZX_ZF_EILAST;

    return 0;
}

int szx_load_block_ay(struct SZXBlock *b, Machine_t *m)
{
    SZXAYBlock_t *a = (SZXAYBlock_t *)b->data;

    for (int i = 0; i < 16; i++) {
        ay_write_address(&m->ay, i);
        ay_write_data(&m->ay, a->ay_regs[i]);
    }

    ay_write_address(&m->ay, a->current_register);

    return 0;
}

int szx_load_block_specregs(struct SZXBlock *b, Machine_t *m)
{
    SZXSpecRegs_t *r = (SZXSpecRegs_t *)b->data;

    ula_set_border(r->border, 0);
    io_port_write(m, 0xfe, r->fe);

    return 0;
}

int szx_state_load(SZX_t *szx, struct Machine *m)
{
    if (szx->header.machine_id != SZX_MID_48K) {
        return -1;
    }

    machine_deinit(m);
    machine_init(m, MACHINE_ZX48K);

    for (size_t i = 0; i < szx->blocks; i++) {
        struct SZXBlock *block = &szx->block[i];
        int err = 0;

        switch (block->header.id)
        {
        case U32_FROM_CH('Z','8','0','R'):
            err = szx_load_block_z80regs(block, m);
            break;
        case U32_FROM_CH('R','A','M','P'):
            err = szx_load_block_rampage(block, m);
            break;
        case U32_FROM_CH('A','Y','\0','\0'):
            err = szx_load_block_ay(block, m);
            break;
        case U32_FROM_CH('S','P','C','R'):
            err = szx_load_block_specregs(block, m);
            break;
        }

        if (err) {
            return -2;
        }
    }

    return 0;
}
