#include "szx_state.h"
#include "szx_blocks.h"
#include "ula.h"
#include "io.h"
#include <string.h>
#include <stdlib.h>
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

int szx_save_block_rampage(struct SZXBlock *b, Machine_t *m, int page_no)
{
    b->header.id = U32_FROM_CH('R','A','M','P');
    b->header.size = sizeof(SZXRAMPage_t) - 1 + 0x4000;

    b->data = calloc(b->header.size, 1);
    if (b->data == NULL) {
        return -1;
    }

    SZXRAMPage_t *page = (SZXRAMPage_t *)b->data;

    page->page_no = page_no;
    uint8_t *src;
    switch (page_no)
    {
    case 5:
        src = &m->memory.bus[0x4000];
        break;
    case 2:
        src = &m->memory.bus[0x8000];
        break;
    case 0:
        src = &m->memory.bus[0xC000];
        break;
    default:
        return -2;
    }

    memcpy(page->data, src, 0x4000);

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

int szx_save_block_z80regs(struct SZXBlock *b, Machine_t *m)
{
    b->header.id = U32_FROM_CH('Z','8','0','R');
    b->header.size = sizeof(SZXZ80Regs_t);

    b->data = calloc(b->header.size, 1);
    if (b->data == NULL) {
        return -1;
    }

    SZXZ80Regs_t *r = (SZXZ80Regs_t *)b->data;
    r->af = m->cpu.regs.main.af;
    r->bc = m->cpu.regs.main.bc;
    r->de = m->cpu.regs.main.de;
    r->hl = m->cpu.regs.main.hl;

    r->af1 = m->cpu.regs.alt.af;
    r->bc1 = m->cpu.regs.alt.bc;
    r->de1 = m->cpu.regs.alt.de;
    r->hl1 = m->cpu.regs.alt.hl;

    r->ix = m->cpu.regs.ix;
    r->iy = m->cpu.regs.iy;
    r->sp = m->cpu.regs.sp;
    r->pc = m->cpu.regs.pc;

    r->i = m->cpu.regs.i;
    r->r = m->cpu.regs.r;

    r->iff1 = m->cpu.regs.iff1;
    r->iff2 = m->cpu.regs.iff2;

    r->im = m->cpu.regs.im;

    r->cycles_start = m->cpu.cycles;
    r->hold_int_req_cycles = 32;
    r->flags |= m->cpu.halted ? SZX_ZF_HALTED : 0;
    r->flags |= m->cpu.last_ei ? SZX_ZF_EILAST : 0;

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

int szx_save_block_ay(struct SZXBlock *b, Machine_t *m)
{
    b->header.id = U32_FROM_CH('A','Y','\0','\0');
    b->header.size = sizeof(SZXAYBlock_t);

    b->data = calloc(b->header.size, 1);
    if (b->data == NULL) {
        return -1;
    }

    SZXAYBlock_t *a = (SZXAYBlock_t *)b->data;

    for (int i = 0; i < 16; i++) {
        a->ay_regs[i] = m->ay.regs[i];
    }

    a->current_register = m->ay.address;
    if (m->type == MACHINE_ZX48K) {
        a->flags |= SZX_AYF_128AY;
    }

    return 0;
}

int szx_load_block_specregs(struct SZXBlock *b, Machine_t *m)
{
    SZXSpecRegs_t *r = (SZXSpecRegs_t *)b->data;

    io_port_write(m, 0xfe, r->fe);
    ula_set_border(r->border, 0);

    return 0;
}

int szx_save_block_specregs(struct SZXBlock *b, Machine_t *m)
{
    b->header.id = U32_FROM_CH('S','P','C','R');
    b->header.size = sizeof(SZXSpecRegs_t);

    b->data = calloc(b->header.size, 1);
    if (b->data == NULL) {
        return -1;
    }

    SZXSpecRegs_t *r = (SZXSpecRegs_t *)b->data;

    r->border = ula_get_border();
    // FIXME: cant get last fe val :(

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

    ula_reset_screen_dirty();

    return 0;
}

SZX_t *szx_state_save(struct Machine *m)
{
    SZX_t *szx = malloc(sizeof(SZX_t));
    
    if (szx == NULL) {
        return NULL;
    }

    szx->blocks = 6;
    szx->block = malloc(sizeof(struct SZXBlock) * szx->blocks);
    if (szx->block == NULL) {
        free(szx);
        return NULL;
    }

    szx->header.magic = U32_FROM_CH('Z','X','S','T');
    szx->header.version_major = 1;
    szx->header.version_minor = 4;

    szx->header.machine_id = SZX_MID_48K;
    szx->header.flags = 0;

    int err;
    err = szx_save_block_z80regs(&szx->block[0], m);
    err = szx_save_block_rampage(&szx->block[1], m, 0);
    err = szx_save_block_rampage(&szx->block[2], m, 2);
    err = szx_save_block_rampage(&szx->block[3], m, 5);
    err = szx_save_block_specregs(&szx->block[4], m);
    err = szx_save_block_ay(&szx->block[5], m);

    if (err) {
        return NULL;
    }

    return szx;
}
