#include "sna.h"
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include "file.h"
#include "ula.h"

struct SNA
{
    bool is_128k;

    uint8_t i;
    uint16_t hl_, de_, bc_, af_;
    uint16_t hl, de, bc, iy, ix;
    uint8_t interrupt;
    uint8_t r;
    uint16_t af, sp;
    uint8_t im;
    uint8_t border;
    uint8_t ram[0x4000*9]; // yes, 9.

    // 128k extensions.
    uint16_t pc;
    uint8_t port_7ffd;
    uint8_t trdos_paged;
    uint16_t bank_offsets[8]; // for convenience.
};

#define SNA_IFF2 (1<<2)
#define SNA_SIZE_48K 49179

#define ALIGN_TO(size, align) (((size+align-1) / align) * align)

static bool error_eof(SDL_IOStream *io)
{
    // SDL is not obligated to set an error message on EOF, so we do it ourselves.
    SDL_IOStatus status = SDL_GetIOStatus(io);
    if (status == SDL_IO_STATUS_EOF || status == SDL_IO_STATUS_READY) {
        return SDL_SetError("File ended prematurely");
    }
    return false;
}

static bool sna_state_read_io(SDL_IOStream *io, struct SNA *sna)
{
    sna->is_128k = false;

    bool ok;
    ok = SDL_ReadU8(io, &sna->i);
    ok = SDL_ReadU16LE(io, &sna->hl_);
    ok = SDL_ReadU16LE(io, &sna->de_);
    ok = SDL_ReadU16LE(io, &sna->bc_);
    ok = SDL_ReadU16LE(io, &sna->af_);
    ok = SDL_ReadU16LE(io, &sna->hl);
    ok = SDL_ReadU16LE(io, &sna->de);
    ok = SDL_ReadU16LE(io, &sna->bc);
    ok = SDL_ReadU16LE(io, &sna->iy);
    ok = SDL_ReadU16LE(io, &sna->ix);
    ok = SDL_ReadU8(io, &sna->interrupt);
    ok = SDL_ReadU8(io, &sna->r);
    ok = SDL_ReadU16LE(io, &sna->af);
    ok = SDL_ReadU16LE(io, &sna->sp);
    ok = SDL_ReadU8(io, &sna->im);
    ok = SDL_ReadU8(io, &sna->border);
    if (!ok) {
        return error_eof(io);
    }

    size_t bytes;
    if ((bytes = SDL_ReadIO(io, sna->ram, 0x4000 * 3)) != 0x4000 * 3) {
        return error_eof(io);
    }

    // at this point we have a valid 48K snapshot file.
    // the only way to tell a 128K one apart is by the size.
    // let's assume the stream is not seekable and just try reading more.
    ok = SDL_ReadU16LE(io, &sna->pc);
    ok = SDL_ReadU8(io, &sna->port_7ffd);
    ok = SDL_ReadU8(io, &sna->trdos_paged);
    if (!ok) {
        return true;
    }

    int page = sna->port_7ffd & 7;

    int banks_remaining = 5;
    if (page == 5 || page == 2) {
        // insanity.
        banks_remaining += 1;
    }

    const size_t bytes_remaining = 0x4000 * banks_remaining;
    if ((bytes = SDL_ReadIO(io, sna->ram + 0x4000 * 3, bytes_remaining)) != bytes_remaining) {
        return true;
    }

    sna->bank_offsets[5] = 0x4000 * 0;
    sna->bank_offsets[2] = 0x4000 * 1;
    const size_t paged_offset = 0x4000 * 2;

    if (page == 5 || page == 2) {
        // this is a duplicated bank, let's see if it checks out anyway.
        if (memcmp(&sna->ram[paged_offset], &sna->ram[sna->bank_offsets[page]], 0x4000)) {
            // insanity.
            return SDL_SetError("Snapshot has a duplicate bank, but the contents do not match. You had one job");
        }
    }

    sna->bank_offsets[page] = paged_offset;

    // remaining banks are arranged in ascending order.
    uint16_t offset = 0x4000 * 3;
    for (int i = 0; i < 8; i++) {
        if (i == 5 || i == 2 || i == page) {
            continue;
        }
        sna->bank_offsets[i] = offset;
        offset += 0x4000;
    }

    sna->is_128k = true;
    return true;
}

bool sna_state_load_io(SDL_IOStream *io, Machine_t *m)
{
    struct SNA *sna = malloc(sizeof(struct SNA));
    if (!sna) {
        return SDL_OutOfMemory();
    }

    if (!sna_state_read_io(io, sna)) {
        free(sna);
        return false;
    }

    if (sna->is_128k) {
        free(sna);
        return SDL_SetError("128K snapshots are not supported");
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
    return true;
}

bool sna_state_load(char *path, Machine_t *m)
{
    SDL_IOStream *io = SDL_IOFromFile(path, "rb");
    if (!io) {
        return false;
    }

    bool result = sna_state_load_io(io, m);
    SDL_CloseIO(io);
    return result;
}
