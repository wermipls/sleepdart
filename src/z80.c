#include "z80.h"
#include "machine.h"
#include "io.h"
#include "log.h"
#include <assert.h>

#include <stdio.h>

/* helpers */

#define SF  (1<<7)
#define ZF  (1<<6)
#define XF  (1<<5)
#define HF  (1<<4)
#define YF  (1<<3)
#define PF  (1<<2)
#define NF  (1<<1)
#define CF  (1<<0)

#define MASK_FLAG_XY    (XF | YF)
#define MAKE16(L, H)    (L | (H << 8))
#define LOW8(HL)        (HL & 255)
#define HIGH8(HL)       (HL >> 8)

static void print_regs(Z80_t *cpu)
{
    uint8_t *bus = cpu->ctx->memory.bus;

    dlog(LOG_INFO, "cycles: %d", cpu->cycles);
    dlog(LOG_INFO, "  %02X %02X %02X %02X %02X",
                   memory_bus_peek(bus, cpu->regs.pc-2),
                   memory_bus_peek(bus, cpu->regs.pc-1),
                   memory_bus_peek(bus, cpu->regs.pc),
                   memory_bus_peek(bus, cpu->regs.pc+1),
                   memory_bus_peek(bus, cpu->regs.pc+2));
    dlog(LOG_INFO, "        ^ PC");
    dlog(LOG_INFO, "  PC  %04X, SP  %04X, IX  %04X, IY  %04X",
                   cpu->regs.pc, cpu->regs.sp, cpu->regs.ix, cpu->regs.iy);
    dlog(LOG_INFO, "  AF  %04X, BC  %04X, DE  %04X, HL  %04X",
                   cpu->regs.main.af, cpu->regs.main.bc,
                   cpu->regs.main.de, cpu->regs.main.hl);
    dlog(LOG_INFO, "  AF` %04X, BC` %04X, DE` %04X, HL` %04X",
                   cpu->regs.alt.af, cpu->regs.alt.bc,
                   cpu->regs.alt.de, cpu->regs.alt.hl);
}

static inline uint8_t cpu_read(Z80_t *cpu, uint16_t addr)
{
    uint8_t value;
    cpu->cycles += memory_read(cpu->ctx, addr, &value);
    return value;
}

static inline void cpu_write(Z80_t *cpu, uint16_t addr, uint8_t value)
{
    cpu->cycles += memory_write(cpu->ctx, addr, value);
}

static inline uint8_t cpu_in(Z80_t *cpu, uint16_t addr)
{
    uint8_t value;
    cpu->cycles += io_port_read(cpu->ctx, addr, &value);
    return value;
}

static inline void cpu_out(Z80_t *cpu, uint16_t addr, uint8_t value)
{
    cpu->cycles += io_port_write(cpu->ctx, addr, value);
}

static inline void cpu_memory_stall(Z80_t *cpu, uint16_t addr, uint8_t count)
{
    while (count--) {
        cpu_read(cpu, addr);
        cpu->cycles += 1;
    }
}

static inline bool get_parity(uint8_t value)
{
    // FIXME: untested HACK
    value = value ^ (value >> 4);
    value = value ^ (value >> 2);
    value = value ^ (value >> 1);
    return !(value & 1);
}

static inline bool flag_overflow_8(uint8_t a, uint8_t b, bool c, bool sub)
{
    int16_t as = (int8_t)a;
    int16_t bs = (int8_t)b;
    int16_t result = sub ? (as - bs - c) : (as + bs + c);
    return result > 127 || result < -128;
}

static inline bool flag_overflow_16(uint16_t a, uint16_t b, bool c, bool sub)
{
    int32_t as = (int16_t)a;
    int32_t bs = (int16_t)b;
    int32_t result = sub ? (as - bs - c) : (as + bs + c);
    return result > 32767 || result < -32768;
}

static inline void inc_refresh(Z80_t *cpu)
{
    uint8_t r = (cpu->regs.r + 1) & 127;
    cpu->regs.r = (cpu->regs.r & (1<<7)) | r;
}

static inline void alo(Z80_t *cpu, uint8_t value, const uint8_t op);
static inline uint8_t sub8(Z80_t *cpu, uint8_t a, uint8_t value);
static inline uint8_t dec8(Z80_t *cpu, uint8_t value);

/* instruction implementations */

/* 8-Bit Load Group */

/* LD r, r */
static void ld_r_r(Z80_t *cpu, uint8_t *dest, uint8_t value)
{
    *dest = value;
    cpu->cycles += 4;
    cpu->regs.pc += 1;
}

/* LD r, (rr) */
static void ld_r_rra(Z80_t *cpu, uint8_t *dest, uint16_t addr)
{
    cpu->cycles += 4;
    *dest = cpu_read(cpu, addr);
    cpu->cycles += 3;
    cpu->regs.pc += 1;
}

static void ld_a_rra(Z80_t *cpu, uint16_t addr)
{
    cpu->cycles += 4;
    cpu->regs.main.a = cpu_read(cpu, addr);
    cpu->regs.memptr = addr+1;
    cpu->cycles += 3;
    cpu->regs.pc += 1;
}

/* LD r, n */
static void ld_r_n(Z80_t *cpu, uint8_t *dest)
{
    cpu->cycles += 4;
    cpu->regs.pc++;
    *dest = cpu_read(cpu, cpu->regs.pc);
    cpu->cycles += 3;
    cpu->regs.pc++;
}

static void ld_a_nna(Z80_t *cpu)
{
    cpu->cycles += 4;
    cpu->regs.pc++;
    uint8_t l = cpu_read(cpu, cpu->regs.pc);
    cpu->cycles += 3;
    cpu->regs.pc++;
    uint8_t h = cpu_read(cpu, cpu->regs.pc);
    cpu->cycles += 3;
    cpu->regs.pc++;
    uint16_t addr = MAKE16(l, h);
    cpu->regs.memptr = addr;
    cpu->regs.main.a = cpu_read(cpu, addr);
    cpu->cycles += 3;
}

/* LD (rr), n */
static void ld_rra_n(Z80_t *cpu, uint16_t addr)
{
    cpu->cycles += 4;
    cpu->regs.pc++;
    uint8_t value = cpu_read(cpu, cpu->regs.pc);
    cpu->cycles += 3;
    cpu->regs.pc++;
    cpu_write(cpu, addr, value);
    cpu->cycles += 3;
}

/* ld i, a */
static void ld_i_a(Z80_t *cpu, uint8_t *dest)
{
    *dest = cpu->regs.main.a;
    cpu->regs.pc++;
    cpu->cycles += 4;
    cpu_read(cpu, MAKE16(cpu->regs.r, cpu->regs.i)); // ir:1
    cpu->cycles += 1;
}

/* ld a, i */
static void ld_a_i(Z80_t *cpu, uint8_t value)
{
    cpu->regs.pc++;
    cpu->cycles += 4;
    cpu_read(cpu, MAKE16(cpu->regs.r, cpu->regs.i)); // ir:1
    cpu->cycles += 1;

    cpu->regs.main.f &= ~MASK_FLAG_XY;
    cpu->regs.main.f |= (value & MASK_FLAG_XY);
    cpu->regs.main.flags.s = value & (1<<7);
    cpu->regs.main.flags.z = !value;
    cpu->regs.main.flags.h = 0;
    cpu->regs.main.flags.pv = cpu->regs.iff2;
    cpu->regs.main.flags.n = 0;
    cpu->regs.q = true;

    cpu->regs.main.a = value;
}

/* LD (nn), a */
static void ld_nna_a(Z80_t *cpu)
{
    cpu->cycles += 4;
    cpu->regs.pc++;
    uint8_t l = cpu_read(cpu, cpu->regs.pc);
    cpu->cycles += 3;
    cpu->regs.pc++;
    uint8_t h = cpu_read(cpu, cpu->regs.pc);
    int16_t addr = MAKE16(l, h);
    cpu->regs.z = (addr + 1) & 0xFF;
    cpu->regs.w = cpu->regs.main.a;
    cpu->cycles += 3;
    cpu->regs.pc++;
    cpu_write(cpu, addr, cpu->regs.main.a);
    cpu->cycles += 3;
}

/* LD (rr), r */
static void ld_rra_r(Z80_t *cpu, uint16_t addr, uint8_t value)
{
    cpu->cycles += 4;
    cpu->regs.pc++;
    cpu_write(cpu, addr, value);
    cpu->cycles += 3;
}

static void ld_rra_a(Z80_t *cpu, uint16_t addr)
{
    cpu->cycles += 4;
    cpu->regs.pc++;
    cpu_write(cpu, addr, cpu->regs.main.a);
    cpu->regs.w = cpu->regs.main.a;
    cpu->regs.z = (addr+1) & 0xFF;
    cpu->cycles += 3;
}

/* LD (ii+d), r */
static void ld_iid_r(Z80_t *cpu, uint16_t addr, uint8_t value)
{
    cpu->cycles += 4;
    cpu->regs.pc++;
    int8_t d = (int8_t)cpu_read(cpu, cpu->regs.pc);
    addr += d;
    cpu->regs.memptr = addr;
    cpu->cycles += 3;

    // pc+2:1 x5
    cpu_memory_stall(cpu, cpu->regs.pc, 5);

    cpu->regs.pc++;
    cpu_write(cpu, addr, value);
    cpu->cycles += 3;
}

/* LD r, (ii+d) */
static void ld_r_iid(Z80_t *cpu, uint8_t *dest, uint16_t addr)
{
    cpu->cycles += 4;
    cpu->regs.pc++;
    int8_t d = (int8_t)cpu_read(cpu, cpu->regs.pc);
    addr += d;
    cpu->regs.memptr = addr;
    cpu->cycles += 3;

    // pc+2:1 x5
    cpu_memory_stall(cpu, cpu->regs.pc, 5);

    cpu->regs.pc++;
    *dest = cpu_read(cpu, addr);
    cpu->cycles += 3;
}

/* LD (ii+d), n */
static void ld_iid_n(Z80_t *cpu, uint16_t addr)
{
    cpu->cycles += 4;
    cpu->regs.pc++;
    int8_t d = (int8_t)cpu_read(cpu, cpu->regs.pc);
    addr += d;
    cpu->regs.memptr = addr;
    cpu->cycles += 3;
    cpu->regs.pc++;
    uint8_t value = cpu_read(cpu, cpu->regs.pc);
    cpu->cycles += 3;

    // pc+3:1 x2
    cpu_memory_stall(cpu, cpu->regs.pc, 2);

    cpu->regs.pc++;
    cpu_write(cpu, addr, value);
    cpu->cycles += 3;
}


/* 16-Bit Load Group */

/* LD rr, nn */
static void ld_rr_nn(Z80_t *cpu, uint16_t *dest)
{
    cpu->cycles += 4;
    cpu->regs.pc++;
    uint8_t l = cpu_read(cpu, cpu->regs.pc);
    cpu->cycles += 3;
    cpu->regs.pc++;
    uint8_t h = cpu_read(cpu, cpu->regs.pc);
    *dest = MAKE16(l, h);
    cpu->cycles += 3;
    cpu->regs.pc++;
}

/* LD rr, (nn) */
static void ld_rr_nna(Z80_t *cpu, uint16_t *dest)
{
    cpu->cycles += 4;
    cpu->regs.pc++;
    uint8_t l = cpu_read(cpu, cpu->regs.pc);
    cpu->cycles += 3;
    cpu->regs.pc++;
    uint8_t h = cpu_read(cpu, cpu->regs.pc);
    uint16_t addr = MAKE16(l, h);
    cpu->cycles += 3;
    cpu->regs.pc++;
    l = cpu_read(cpu, addr);
    cpu->cycles += 3;
    h = cpu_read(cpu, ++addr);
    cpu->regs.memptr = addr;
    cpu->cycles += 3;
    *dest = MAKE16(l, h);
}

/* LD (nn), rr */
static void ld_nna_rr(Z80_t *cpu, uint16_t value)
{
    cpu->cycles += 4;
    cpu->regs.pc++;
    uint8_t l = cpu_read(cpu, cpu->regs.pc);
    cpu->cycles += 3;
    cpu->regs.pc++;
    uint8_t h = cpu_read(cpu, cpu->regs.pc);
    int16_t addr = MAKE16(l, h);
    cpu->cycles += 3;
    cpu->regs.pc++;
    cpu_write(cpu, addr, LOW8(value));
    cpu->cycles += 3;
    cpu_write(cpu, ++addr, HIGH8(value));
    cpu->cycles += 3;
    cpu->regs.memptr = addr;
}

static void ld_sp_rr(Z80_t *cpu, uint16_t value)
{
    cpu->cycles += 4;
    // ir:1 x2
    cpu_memory_stall(cpu, MAKE16(cpu->regs.r, cpu->regs.i), 2);

    cpu->regs.pc++;
    cpu->regs.sp = value;
}

static void pop(Z80_t *cpu, uint16_t *dest)
{
    cpu->cycles += 4;
    cpu->regs.pc++;
    uint8_t l = cpu_read(cpu, cpu->regs.sp);
    cpu->cycles += 3;
    cpu->regs.sp++;
    uint8_t h = cpu_read(cpu, cpu->regs.sp);
    cpu->cycles += 3;
    cpu->regs.sp++;
    *dest = MAKE16(l, h);
}

static void ret(Z80_t *cpu)
{
    pop(cpu, &cpu->regs.pc);
}

static void retn(Z80_t *cpu)
{
    pop(cpu, &cpu->regs.pc);
    cpu->regs.iff1 = cpu->regs.iff2;
}

static void reti(Z80_t *cpu)
{
    // don't care about correct behavior since it's irrelevant on speccy
    pop(cpu, &cpu->regs.pc);
}

static void push(Z80_t *cpu, uint16_t value)
{
    cpu->cycles += 4;
    cpu_read(cpu, MAKE16(cpu->regs.r, cpu->regs.i)); // ir:1
    cpu->cycles += 1;
    cpu->regs.pc++;
    cpu->regs.sp--;
    cpu_write(cpu, cpu->regs.sp, HIGH8(value));
    cpu->cycles += 3;
    cpu->regs.sp--;
    cpu_write(cpu, cpu->regs.sp, LOW8(value));
    cpu->cycles += 3;
}

/* Exchange, Block Transfer and Search Group */

static void exx(Z80_t *cpu)
{
    uint16_t tmp;
    tmp = cpu->regs.main.bc;
    cpu->regs.main.bc = cpu->regs.alt.bc;
    cpu->regs.alt.bc = tmp;

    tmp = cpu->regs.main.de;
    cpu->regs.main.de = cpu->regs.alt.de;
    cpu->regs.alt.de = tmp;

    tmp = cpu->regs.main.hl;
    cpu->regs.main.hl = cpu->regs.alt.hl;
    cpu->regs.alt.hl = tmp;

    cpu->cycles += 4;
    cpu->regs.pc++;
}

static void ex_de_hl(Z80_t *cpu)
{
    uint16_t tmp;
    tmp = cpu->regs.main.hl;
    cpu->regs.main.hl = cpu->regs.main.de;
    cpu->regs.main.de = tmp;

    cpu->cycles += 4;
    cpu->regs.pc++;
}

static void ex_af(Z80_t *cpu)
{
    uint16_t tmp;
    tmp = cpu->regs.main.af;
    cpu->regs.main.af = cpu->regs.alt.af;
    cpu->regs.alt.af = tmp;

    cpu->cycles += 4;
    cpu->regs.pc++;
}

static void ex_spa_rr(Z80_t *cpu, uint16_t *dest)
{
    cpu->cycles += 4;
    cpu->regs.pc++;
    uint8_t l = cpu_read(cpu, cpu->regs.sp);
    cpu->cycles += 3;
    uint8_t h = cpu_read(cpu, cpu->regs.sp+1);
    cpu->cycles += 3;
    cpu_read(cpu, cpu->regs.sp+1); // sp+1:1
    cpu->cycles += 1;
    cpu_write(cpu, cpu->regs.sp+1, HIGH8(*dest));
    cpu->cycles += 3;
    cpu_write(cpu, cpu->regs.sp, LOW8(*dest));
    cpu->cycles += 3;
    // sp(write):1 x2
    cpu_memory_stall(cpu, cpu->regs.sp, 2);

    cpu->regs.memptr = *dest = MAKE16(l, h);
}

/* LDI/LDD */
static void ldx(Z80_t *cpu, int8_t increment)
{
    cpu->cycles += 4;
    cpu->regs.pc++;
    uint8_t value = cpu_read(cpu, cpu->regs.main.hl);
    cpu->cycles += 3;
    cpu_write(cpu, cpu->regs.main.de, value);
    cpu->cycles += 3;

    // de:1 x2
    cpu_memory_stall(cpu, cpu->regs.main.de, 2);

    cpu->regs.main.hl += increment;
    cpu->regs.main.de += increment;
    cpu->regs.main.bc--;

    uint8_t n = value + cpu->regs.main.a;
    cpu->regs.main.flags.y = n & (1<<1);
    cpu->regs.main.flags.x = n & (1<<3);
    cpu->regs.main.flags.pv = !(!cpu->regs.main.bc);
    cpu->regs.main.flags.h = 0;
    cpu->regs.main.flags.n = 0;
    cpu->regs.q = true;
}

/* LDIR/LDDR */
static void ldxr(Z80_t *cpu, int8_t increment)
{
    ldx(cpu, increment);

    if (cpu->regs.main.bc != 0) {
        cpu->regs.pc -= 2;
        cpu->regs.memptr = cpu->regs.pc;
        // de:1 x5
        cpu_memory_stall(cpu, cpu->regs.main.de, 5);
    }
}

/* CPI/CPD */
static void cpx(Z80_t *cpu, int8_t increment)
{
    cpu->cycles += 4;
    cpu->regs.pc++;
    uint8_t value = cpu_read(cpu, cpu->regs.main.hl);
    cpu->cycles += 3;
    cpu->regs.main.bc--;
    cpu->regs.main.hl += increment;
    cpu->regs.memptr += increment;

    // hl:1 x5
    cpu_memory_stall(cpu, cpu->regs.main.hl, 5);

    bool c = cpu->regs.main.flags.c;
    alo(cpu, value, 7);
    uint8_t n = cpu->regs.main.a - value - cpu->regs.main.flags.h;
    cpu->regs.main.flags.y = n & (1<<1);
    cpu->regs.main.flags.x = n & (1<<3); 
    cpu->regs.main.flags.pv = !(!cpu->regs.main.bc);
    cpu->regs.main.flags.c = c;
    cpu->regs.q = true;
}

/* CPIR/CPDR */
static void cpxr(Z80_t *cpu, int8_t increment)
{
    cpx(cpu, increment);

    if (cpu->regs.main.bc != 0 && !cpu->regs.main.flags.z) {
        cpu->regs.pc -= 2;
        cpu->regs.memptr = cpu->regs.pc + 1;
        // hl:1 x5
        cpu_memory_stall(cpu, cpu->regs.main.hl, 5);
    }
}

/* OUTI/OUTD */
static void outx(Z80_t *cpu, int8_t increment)
{
    cpu->cycles += 4;
    cpu->regs.pc++;
    cpu_read(cpu, MAKE16(cpu->regs.r, cpu->regs.i)); // ir:1
    cpu->cycles += 1;
    uint8_t value = cpu_read(cpu, cpu->regs.main.hl);
    cpu->cycles += 3;
    cpu_out(cpu, cpu->regs.main.bc, value);
    cpu->cycles += 4;
    cpu->regs.main.hl += increment;

    cpu->regs.main.b = dec8(cpu, cpu->regs.main.b);

    cpu->regs.memptr = cpu->regs.main.bc + increment;

    uint16_t k = cpu->regs.main.l + value;
    cpu->regs.main.flags.h = k > 255;
    cpu->regs.main.flags.c = k > 255;
    cpu->regs.main.flags.pv = get_parity((k & 7) ^ cpu->regs.main.b);
    cpu->regs.main.flags.n = value & (1<<7);
    cpu->regs.q = true;
}

/* OTIR/OTDR */
static void otxr(Z80_t *cpu, int8_t increment)
{
    outx(cpu, increment);

    if (cpu->regs.main.b != 0) {
        cpu->regs.pc -= 2;
        // bc:1 x5
        cpu_memory_stall(cpu, cpu->regs.main.bc, 5);
    }
}

/* INI/IND */
static void inx(Z80_t *cpu, int8_t increment)
{
    cpu->cycles += 4;
    cpu->regs.pc++;
    cpu_read(cpu, MAKE16(cpu->regs.r, cpu->regs.i)); // ir:1
    cpu->cycles += 1;

    cpu->regs.memptr = cpu->regs.main.bc + increment;

    cpu->regs.main.b = dec8(cpu, cpu->regs.main.b);

    uint8_t value = cpu_in(cpu, cpu->regs.main.bc);
    cpu->cycles += 4;
    cpu_write(cpu, cpu->regs.main.hl, value);
    cpu->cycles += 3;
    cpu->regs.main.hl += increment;

    uint16_t k = ((cpu->regs.main.c + increment) & 255) + value;
    cpu->regs.main.flags.h = k > 255;
    cpu->regs.main.flags.c = k > 255;
    cpu->regs.main.flags.pv = get_parity((k & 7) ^ cpu->regs.main.b);
    cpu->regs.main.flags.n = value & (1<<7);
    cpu->regs.q = true;
}

/* INIR/INDR */
static void inxr(Z80_t *cpu, int8_t increment)
{
    inx(cpu, increment);

    if (cpu->regs.main.b != 0) {
        cpu->regs.pc -= 2;
        // hl:1 x5
        cpu_memory_stall(cpu, cpu->regs.main.hl, 5);
    }
}

/* 8-Bit Arithmetic Group */

/* INC helper */
static inline uint8_t inc8(Z80_t *cpu, uint8_t value)
{
    value++;
    cpu->regs.main.f &= ~MASK_FLAG_XY;
    cpu->regs.main.f |= (value & MASK_FLAG_XY);
    cpu->regs.main.flags.s = value & (1<<7);
    cpu->regs.main.flags.z = !value;
    cpu->regs.main.flags.h = !(value & 0x0F);
    cpu->regs.main.flags.pv = value == 0x80;
    cpu->regs.main.flags.n = 0;
    cpu->regs.q = true;
    return value;
}

/* INC r */
static void inc_r(Z80_t *cpu, uint8_t *dest)
{
    cpu->cycles += 4;
    cpu->regs.pc++;
    *dest = inc8(cpu, *dest);
}

/* INC (rr) */
static void inc_rra(Z80_t *cpu, uint16_t addr)
{
    cpu->cycles += 4;
    cpu->regs.pc++;
    uint8_t value = cpu_read(cpu, addr);
    cpu->cycles += 3;
    cpu_read(cpu, addr); // hl:1
    cpu->cycles += 1;
    value = inc8(cpu, value);
    cpu_write(cpu, addr, value);
    cpu->cycles += 3;
}

/* INC (ii+d) */
static void inc_iid(Z80_t *cpu, uint16_t addr)
{
    cpu->cycles += 4;
    cpu->regs.pc++;
    int8_t d = (int8_t)cpu_read(cpu, cpu->regs.pc);
    cpu->cycles += 3;

    // pc+2:1 x5
    cpu_memory_stall(cpu, cpu->regs.pc, 5);

    cpu->regs.pc++;
    addr += d;
    cpu->regs.memptr = addr;
    uint8_t value = cpu_read(cpu, addr);
    cpu->cycles += 3;
    cpu_read(cpu, addr); // ii+n:1
    cpu->cycles += 1;
    value = inc8(cpu, value);
    cpu_write(cpu, addr, value);
    cpu->cycles += 3;
}

/* DEC helper */
static inline uint8_t dec8(Z80_t *cpu, uint8_t value)
{
    value--;
    cpu->regs.main.f &= ~MASK_FLAG_XY;
    cpu->regs.main.f |= (value & MASK_FLAG_XY);
    cpu->regs.main.flags.s = value & (1<<7);
    cpu->regs.main.flags.z = !value;
    cpu->regs.main.flags.h = (value & 0x0F) == 0x0F;
    cpu->regs.main.flags.pv = value == 0x7F;
    cpu->regs.main.flags.n = 1;
    cpu->regs.q = true;
    return value;
}

/* DEC r */
static void dec_r(Z80_t *cpu, uint8_t *dest)
{
    cpu->cycles += 4;
    cpu->regs.pc++;
    *dest = dec8(cpu, *dest);
}

/* DEC (rr) */
static void dec_rra(Z80_t *cpu, uint16_t addr)
{
    cpu->cycles += 4;
    cpu->regs.pc++;
    uint8_t value = cpu_read(cpu, addr);
    cpu->cycles += 3;
    cpu_read(cpu, addr); // hl:1
    cpu->cycles += 1;
    value = dec8(cpu, value);
    cpu_write(cpu, addr, value);
    cpu->cycles += 3;
}

/* DEC (ii+d) */
static void dec_iid(Z80_t *cpu, uint16_t addr)
{
    cpu->cycles += 4;
    cpu->regs.pc++;
    int8_t d = (int8_t)cpu_read(cpu, cpu->regs.pc);
    cpu->cycles += 3;

    // pc+2:1 x5
    cpu_memory_stall(cpu, cpu->regs.pc, 5);

    cpu->regs.pc++;
    addr += d;
    cpu->regs.memptr = addr;
    uint8_t value = cpu_read(cpu, addr);
    cpu->cycles += 3;
    cpu_read(cpu, addr); // ii+n:1
    cpu->cycles += 1;
    value = dec8(cpu, value);
    cpu_write(cpu, addr, value);
    cpu->cycles += 3;
}

/* ADD helper */
static inline uint8_t add8(Z80_t *cpu, uint8_t value)
{
    uint8_t a = cpu->regs.main.a;
    uint8_t result = a + value;
    cpu->regs.main.flags.pv = flag_overflow_8(a, value, 0, false);
    cpu->regs.main.flags.h = (a ^ result ^ value) & 0x10;
    cpu->regs.main.flags.c = ((uint16_t)a + (uint16_t)value) > 255;
    cpu->regs.main.flags.n = 0;
    return result;
}

/* ADC helper */
static inline uint8_t adc8(Z80_t *cpu, uint8_t value)
{
    uint8_t a = cpu->regs.main.a;
    uint8_t result = a + value + cpu->regs.main.flags.c;
    cpu->regs.main.flags.pv = flag_overflow_8(a, value, cpu->regs.main.flags.c, false);
    cpu->regs.main.flags.h = (a ^ result ^ value) & 0x10;
    cpu->regs.main.flags.c = ((uint16_t)a + (uint16_t)value + 
                              cpu->regs.main.flags.c) > 255;
    cpu->regs.main.flags.n = 0;
    return result;
}

/* SUB helper */
static inline uint8_t sub8(Z80_t *cpu, uint8_t a, uint8_t value)
{
    uint8_t result = a - value;
    cpu->regs.main.flags.pv = flag_overflow_8(a, value, 0, true);
    cpu->regs.main.flags.h = (a ^ result ^ value) & 0x10;
    cpu->regs.main.flags.c = (a < value);
    cpu->regs.main.flags.n = 1;
    return result;
}

/* SBC helper */
static inline uint8_t sbc8(Z80_t *cpu, uint8_t value)
{
    uint8_t a = cpu->regs.main.a;
    uint8_t result = a - value - cpu->regs.main.flags.c;
    cpu->regs.main.flags.pv = flag_overflow_8(a, value, cpu->regs.main.flags.c, true);
    cpu->regs.main.flags.h = (a ^ result ^ value) & 0x10;
    cpu->regs.main.flags.c = a < ((uint16_t)value + cpu->regs.main.flags.c);
    cpu->regs.main.flags.n = 1;
    return result;
}

/* AND helper */
static inline uint8_t and(Z80_t *cpu, uint8_t value)
{
    value &= cpu->regs.main.a;
    cpu->regs.main.flags.h = 1;
    cpu->regs.main.flags.pv = get_parity(value);
    cpu->regs.main.flags.c = 0;
    cpu->regs.main.flags.n = 0;
    return value;
}

/* XOR helper */
static inline uint8_t xor(Z80_t *cpu, uint8_t value)
{
    value ^= cpu->regs.main.a;
    cpu->regs.main.flags.h = 0;
    cpu->regs.main.flags.pv = get_parity(value);
    cpu->regs.main.flags.c = 0;
    cpu->regs.main.flags.n = 0;
    return value;
}

/* OR helper */
static inline uint8_t or(Z80_t *cpu, uint8_t value)
{
    value |= cpu->regs.main.a;
    cpu->regs.main.flags.h = 0;
    cpu->regs.main.flags.pv = get_parity(value);
    cpu->regs.main.flags.c = 0;
    cpu->regs.main.flags.n = 0;
    return value;
}

static inline void alo(Z80_t *cpu, uint8_t value, const uint8_t op)
{
    uint8_t result;
    switch (op)
    {
    case 0: // add
        result = cpu->regs.main.a = add8(cpu, value);
        break;
    case 1: // adc
        result = cpu->regs.main.a = adc8(cpu, value);
        break;
    case 2: // sub
        result = cpu->regs.main.a = sub8(cpu, cpu->regs.main.a, value);
        break;
    case 3: // sbc
        result = cpu->regs.main.a = sbc8(cpu, value);
        break;
    case 4: // and
        result = cpu->regs.main.a = and(cpu, value);
        break;
    case 5: // xor
        result = cpu->regs.main.a = xor(cpu, value);
        break;
    case 6: // or
        result = cpu->regs.main.a = or(cpu, value);
        break;
    case 7: // cp
        result = sub8(cpu, cpu->regs.main.a, value);
        break;
    default:
        result = 0;
        cpu->error = true;
    }

    uint8_t xy = (op == 7) ? value : result;
    cpu->regs.main.f &= ~MASK_FLAG_XY;
    cpu->regs.main.f |= (xy & MASK_FLAG_XY);
    cpu->regs.main.flags.s = result & (1<<7);
    cpu->regs.main.flags.z = !result;
    cpu->regs.q = true;
}

static void alo_r(Z80_t *cpu, uint8_t value, uint8_t op)
{
    cpu->cycles += 4;
    cpu->regs.pc++;
    alo(cpu, value, op);
}

static void alo_n(Z80_t *cpu, uint8_t op)
{
    cpu->cycles += 4;
    cpu->regs.pc++;
    uint8_t value = cpu_read(cpu, cpu->regs.pc);
    cpu->cycles += 3;
    cpu->regs.pc++;
    alo(cpu, value, op);
}

static void alo_rra(Z80_t *cpu, uint16_t addr, uint8_t op)
{
    cpu->cycles += 4;
    cpu->regs.pc++;
    uint8_t value = cpu_read(cpu, addr);
    cpu->cycles += 3;
    alo(cpu, value, op);
}

/* ADD a, (ii+d) */
static void alo_iid(Z80_t *cpu, uint16_t addr, uint8_t op)
{
    cpu->cycles += 4;
    cpu->regs.pc++;
    int8_t d = (int8_t)cpu_read(cpu, cpu->regs.pc);
    addr += d;
    cpu->regs.memptr = addr;
    cpu->cycles += 3;

    // pc+2:1 x5
    cpu_memory_stall(cpu, cpu->regs.pc, 5);

    cpu->regs.pc++;
    uint8_t value = cpu_read(cpu, addr);
    cpu->cycles += 3;
    alo(cpu, value, op);
}

/* General-Purpose Arithmetic and CPU Control Groups */

static void daa(Z80_t *cpu)
{
    cpu->cycles += 4;
    cpu->regs.pc++;

    uint8_t a = cpu->regs.main.a;
    bool neg = cpu->regs.main.flags.n;
    uint8_t ah = a >> 4;
    uint8_t al = a & 0xF;
    uint8_t diff_lo = cpu->regs.main.flags.h || (al > 9);
    uint8_t diff_hi = cpu->regs.main.flags.c || (ah > 9)
                      || (ah > 8 && al > 9);
    diff_lo *= 0x6;
    diff_hi *= 0x60;
    uint8_t diff = diff_lo | diff_hi;

    bool c = cpu->regs.main.flags.c || (ah > (9 - (al > 9)));
    bool h = (!neg && (al > 9))
             || (neg && cpu->regs.main.flags.h && (al < 6));

    uint8_t result = neg ? (a - diff) : (a + diff);

    const uint8_t fmask = XF | SF | YF;
    cpu->regs.main.f &= ~fmask;
    cpu->regs.main.f |= result & fmask;
    cpu->regs.main.flags.z = !result;
    cpu->regs.main.flags.c = c;
    cpu->regs.main.flags.h = h;
    cpu->regs.main.flags.pv = get_parity(result);
    cpu->regs.q = true;

    cpu->regs.main.a = result;
}

static void cpl(Z80_t *cpu)
{
    cpu->cycles += 4;
    cpu->regs.pc++;
    cpu->regs.main.a ^= 0xFF;
    cpu->regs.main.f &= ~MASK_FLAG_XY;
    cpu->regs.main.f |= (cpu->regs.main.a & MASK_FLAG_XY);
    cpu->regs.main.flags.h = 1;
    cpu->regs.main.flags.n = 1;
    cpu->regs.q = true;
}

static void neg(Z80_t *cpu)
{
    cpu->cycles += 4;
    cpu->regs.pc++;
    uint8_t result = sub8(cpu, 0, cpu->regs.main.a);
    cpu->regs.main.f &= ~MASK_FLAG_XY;
    cpu->regs.main.f |= (result & MASK_FLAG_XY);
    cpu->regs.main.flags.s = result & (1<<7);
    cpu->regs.main.flags.z = !result;
    cpu->regs.main.flags.c = !(!cpu->regs.main.a);
    cpu->regs.main.flags.pv = (cpu->regs.main.a == 0x80);
    cpu->regs.q = true;
    cpu->regs.main.a = result;
}

/* CCF/SCF */
static void ccf(Z80_t *cpu)
{
    cpu->cycles += 4;
    cpu->regs.pc++;
    cpu->regs.main.flags.h = cpu->regs.main.flags.c;
    cpu->regs.main.flags.n = 0;
    cpu->regs.main.flags.c ^= 1;
    if (cpu->regs.q_old) cpu->regs.main.f &= ~MASK_FLAG_XY;
    cpu->regs.main.f |= cpu->regs.main.a & MASK_FLAG_XY;
    cpu->regs.q = true;
}

static void scf(Z80_t *cpu)
{
    cpu->cycles += 4;
    cpu->regs.pc++;
    cpu->regs.main.flags.h = 0;
    cpu->regs.main.flags.n = 0;
    cpu->regs.main.flags.c = 1;
    if (cpu->regs.q_old) cpu->regs.main.f &= ~MASK_FLAG_XY;
    cpu->regs.main.f |= cpu->regs.main.a & MASK_FLAG_XY;
    cpu->regs.q = true;
}

/* IM 0/1/2 */
static void im(Z80_t *cpu, uint8_t im)
{
    cpu->cycles += 4;
    cpu->regs.pc++;
    cpu->regs.im = im;
}


/* 16-Bit Arithmetic Group */

static void inc_rr(Z80_t *cpu, uint16_t *dest)
{
    cpu->cycles += 4;
    // ir:1 x2
    cpu_memory_stall(cpu, MAKE16(cpu->regs.r, cpu->regs.i), 2);

    cpu->regs.pc++;
    *dest += 1;
}

static void dec_rr(Z80_t *cpu, uint16_t *dest)
{
    cpu->cycles += 4;
    // ir:1 x2
    cpu_memory_stall(cpu, MAKE16(cpu->regs.r, cpu->regs.i), 2);

    cpu->regs.pc++;
    *dest -= 1;
}

static void add_rr_rr(Z80_t *cpu, uint16_t *dest, uint16_t value)
{
    cpu->regs.memptr = *dest + 1;
    uint32_t result = (uint32_t)*dest + value;
    cpu->regs.main.f &= ~MASK_FLAG_XY;
    cpu->regs.main.f |= (result >> 8) & MASK_FLAG_XY;
    cpu->regs.main.flags.h = (*dest ^ result ^ value) & 0x1000;
    cpu->regs.main.flags.n = 0;
    cpu->regs.main.flags.c = result & (1<<16); // hmm thats kinda stupid
    cpu->regs.q = true;
    *dest = (uint16_t)result;
    cpu->cycles += 4;

    // ir:1 x7
    cpu_memory_stall(cpu, MAKE16(cpu->regs.r, cpu->regs.i), 7);

    cpu->regs.pc++;
}

static void adc_rr_rr(Z80_t *cpu, uint16_t *dest, uint16_t value)
{
    cpu->regs.memptr = *dest + 1;
    uint32_t result = (uint32_t)*dest + value + cpu->regs.main.flags.c;
    cpu->regs.main.f &= ~MASK_FLAG_XY;
    cpu->regs.main.f |= (result >> 8) & MASK_FLAG_XY;
    cpu->regs.main.flags.s = result & (1<<15);
    cpu->regs.main.flags.z = !(result & 0xFFFF);
    cpu->regs.main.flags.h = (*dest ^ result ^ value) & 0x1000;
    cpu->regs.main.flags.pv = flag_overflow_16(*dest, value, cpu->regs.main.flags.c, false);
    cpu->regs.main.flags.n = 0;
    cpu->regs.main.flags.c = result & (1<<16); // hmm thats kinda stupid
    cpu->regs.q = true;
    *dest = (uint16_t)result;
    cpu->cycles += 4;

    // ir:1 x7
    cpu_memory_stall(cpu, MAKE16(cpu->regs.r, cpu->regs.i), 7);

    cpu->regs.pc++;
}

static void sbc_rr_rr(Z80_t *cpu, uint16_t *dest, uint16_t value)
{
    cpu->regs.memptr = *dest + 1;
    uint32_t result = (uint32_t)*dest - value - cpu->regs.main.flags.c;
    cpu->regs.main.f &= ~MASK_FLAG_XY;
    cpu->regs.main.f |= (result >> 8) & MASK_FLAG_XY;
    cpu->regs.main.flags.s = result & (1<<15);
    cpu->regs.main.flags.z = !(result & 0xFFFF);
    cpu->regs.main.flags.h = (*dest ^ result ^ value) & 0x1000;
    cpu->regs.main.flags.pv = flag_overflow_16(*dest, value, cpu->regs.main.flags.c, true);
    cpu->regs.main.flags.n = 1;
    cpu->regs.main.flags.c = *dest < ((uint32_t)value + cpu->regs.main.flags.c);
    cpu->regs.q = true;
    *dest = (uint16_t)result;
    cpu->cycles += 4;

    // ir:1 x7
    cpu_memory_stall(cpu, MAKE16(cpu->regs.r, cpu->regs.i), 7);

    cpu->regs.pc++;
}

/* Rotate and Shift Group */

static inline uint8_t sro(Z80_t *cpu, uint8_t value, uint8_t op)
{
    bool c_new;
    switch (op)
    {
    case 0: // rlc
        value = (value<<1) | (value>>7);
        cpu->regs.main.flags.c = value & 1;
        break;
    case 1: // rrc
        cpu->regs.main.flags.c = value & 1;
        value = (value>>1) | (value<<7);
        break;
    case 2: // rl
        c_new = (value>>7);
        value = (value<<1) | cpu->regs.main.flags.c;
        cpu->regs.main.flags.c = c_new;
        break;
    case 3: // rr
        c_new = value & 1;
        value = (value>>1) | (cpu->regs.main.flags.c<<7);
        cpu->regs.main.flags.c = c_new;
        break;
    case 4: // sla
        cpu->regs.main.flags.c = value>>7;
        value = (value<<1);
        break;
    case 5:
        cpu->regs.main.flags.c = value & 1;
        value = ((value & (1<<7)) | (value>>1));
        break;
    case 6:
        cpu->regs.main.flags.c = value>>7;
        value = (value<<1) | 1;
        break;
    case 7:
        cpu->regs.main.flags.c = value & 1;
        value = (value>>1);
        break;
    }

    cpu->regs.main.f &= ~MASK_FLAG_XY;
    cpu->regs.main.f |= (value & MASK_FLAG_XY);
    cpu->regs.main.flags.s = value & (1<<7);
    cpu->regs.main.flags.z = !value;
    cpu->regs.main.flags.h = 0;
    cpu->regs.main.flags.pv = get_parity(value);
    cpu->regs.main.flags.n = 0;
    cpu->regs.q = true;

    return value;
}

static void rlca(Z80_t *cpu)
{
    cpu->cycles += 4;
    cpu->regs.pc++;

    uint8_t value = cpu->regs.main.a;
    value = (value<<1) | (value>>7);
    cpu->regs.main.flags.c = value & 1;
    cpu->regs.main.a = value;

    cpu->regs.main.f &= ~MASK_FLAG_XY;
    cpu->regs.main.f |= (value & MASK_FLAG_XY);
    cpu->regs.main.flags.h = 0;
    cpu->regs.main.flags.n = 0;
    cpu->regs.q = true;
}

static void rrca(Z80_t *cpu)
{
    cpu->cycles += 4;
    cpu->regs.pc++;

    uint8_t value = cpu->regs.main.a;
    cpu->regs.main.flags.c = value & 1;
    value = (value>>1) | (value<<7);
    cpu->regs.main.a = value;

    cpu->regs.main.f &= ~MASK_FLAG_XY;
    cpu->regs.main.f |= (value & MASK_FLAG_XY);
    cpu->regs.main.flags.h = 0;
    cpu->regs.main.flags.n = 0;
    cpu->regs.q = true;
}

static void rla(Z80_t *cpu)
{
    cpu->cycles += 4;
    cpu->regs.pc++;

    uint8_t value = cpu->regs.main.a;
    bool c_new = (value>>7);
    value = (value<<1) | cpu->regs.main.flags.c;
    cpu->regs.main.flags.c = c_new;
    cpu->regs.main.a = value;

    cpu->regs.main.f &= ~MASK_FLAG_XY;
    cpu->regs.main.f |= (value & MASK_FLAG_XY);
    cpu->regs.main.flags.h = 0;
    cpu->regs.main.flags.n = 0;
    cpu->regs.q = true;
}

static void rra(Z80_t *cpu)
{
    cpu->cycles += 4;
    cpu->regs.pc++;

    uint8_t value = cpu->regs.main.a;
    bool c_new = value & 1;
    value = (value>>1) | (cpu->regs.main.flags.c<<7);
    cpu->regs.main.flags.c = c_new;
    cpu->regs.main.a = value;

    cpu->regs.main.f &= ~MASK_FLAG_XY;
    cpu->regs.main.f |= (value & MASK_FLAG_XY);
    cpu->regs.main.flags.h = 0;
    cpu->regs.main.flags.n = 0;
    cpu->regs.q = true;
}

static void sro_r(Z80_t *cpu, uint8_t *dest, uint8_t op)
{
    cpu->cycles += 4;
    cpu->regs.pc++;
    *dest = sro(cpu, *dest, op);
}

static void sro_rra(Z80_t *cpu, uint16_t addr, uint8_t op)
{
    cpu->cycles += 4;
    cpu->regs.pc++;
    uint8_t value = cpu_read(cpu, addr);
    cpu->cycles += 3;
    cpu_read(cpu, addr); // hl:1
    cpu->cycles += 1;
    value = sro(cpu, value, op);
    cpu_write(cpu, addr, value);
    cpu->cycles += 3;
}

static void sro_iid_r(Z80_t *cpu, uint16_t addr, uint8_t *dest, uint8_t op)
{
    cpu->cycles += 3;
    // pc+3:1 x2
    cpu_memory_stall(cpu, cpu->regs.pc, 2);

    cpu->regs.pc++;
    uint8_t value = cpu_read(cpu, addr);
    cpu->cycles += 3;
    cpu_read(cpu, addr); // ii+d:1
    cpu->cycles += 1;
    value = sro(cpu, value, op);
    cpu_write(cpu, addr, value);
    cpu->cycles += 3;
    if (dest) *dest = value;
}

static void rld(Z80_t *cpu)
{
    cpu->cycles += 4;
    cpu->regs.pc++;
    uint8_t value = cpu_read(cpu, cpu->regs.main.hl);
    cpu->cycles += 3;

    // hl:1 x4
    cpu_memory_stall(cpu, cpu->regs.main.hl, 4);

    uint8_t a = value >> 4;
    value = value << 4 | (cpu->regs.main.a & 0xF);
    a |= cpu->regs.main.a & 0xF0;
    cpu->regs.main.a = a;

    const uint8_t fmask = XF | SF | YF;
    cpu->regs.main.f &= ~fmask;
    cpu->regs.main.f |= a & fmask;
    cpu->regs.main.flags.z = !a;
    cpu->regs.main.flags.pv = get_parity(a);
    cpu->regs.main.flags.h = 0;
    cpu->regs.main.flags.n = 0;
    cpu->regs.q = true;

    cpu->regs.memptr = cpu->regs.main.hl + 1;

    cpu_write(cpu, cpu->regs.main.hl, value);
    cpu->cycles += 3;
}

static void rrd(Z80_t *cpu)
{
    cpu->cycles += 4;
    cpu->regs.pc++;
    uint8_t value = cpu_read(cpu, cpu->regs.main.hl);
    cpu->cycles += 3;

    // hl:1 x4
    cpu_memory_stall(cpu, cpu->regs.main.hl, 4);

    uint8_t a = value & 0xF;
    value = value >> 4 | ((cpu->regs.main.a & 0xF) << 4);
    a |= cpu->regs.main.a & 0xF0;
    cpu->regs.main.a = a;

    const uint8_t fmask = XF | SF | YF;
    cpu->regs.main.f &= ~fmask;
    cpu->regs.main.f |= a & fmask;
    cpu->regs.main.flags.z = !a;
    cpu->regs.main.flags.pv = get_parity(a);
    cpu->regs.main.flags.h = 0;
    cpu->regs.main.flags.n = 0;
    cpu->regs.q = true;

    cpu->regs.memptr = cpu->regs.main.hl + 1;

    cpu_write(cpu, cpu->regs.main.hl, value);
    cpu->cycles += 3;
}


/* Bit Set, Reset and Test Group */

static inline void bit_(Z80_t *cpu, uint8_t value, uint8_t bit)
{
    uint8_t mask = (1<<bit);
    value &= mask;
    cpu->regs.main.flags.s = value & (1<<7);
    cpu->regs.main.flags.z = !value;
    cpu->regs.main.flags.h = 1;
    cpu->regs.main.flags.pv = get_parity(value);
    cpu->regs.main.flags.n = 0;
    cpu->regs.q = true;
}

static void bit_r(Z80_t *cpu, uint8_t value, uint8_t bit)
{
    cpu->cycles += 4;
    cpu->regs.pc++;
    bit_(cpu, value, bit);
    cpu->regs.main.f &= ~MASK_FLAG_XY;
    cpu->regs.main.f |= (value & MASK_FLAG_XY);
}

static void bit_rra(Z80_t *cpu, uint16_t addr, uint8_t bit)
{
    cpu->cycles += 4;
    cpu->regs.pc++;
    uint8_t value = cpu_read(cpu, addr);
    cpu->cycles += 3;
    cpu_read(cpu, addr); // hl:1
    cpu->cycles += 1;
    bit_(cpu, value, bit);
    cpu->regs.main.f &= ~MASK_FLAG_XY;
    cpu->regs.main.f |= (cpu->regs.w & MASK_FLAG_XY);
}

static void bit_iid(Z80_t *cpu, uint16_t addr, uint8_t bit)
{
    cpu->cycles += 3;
    // pc+3:1 x2
    cpu_memory_stall(cpu, cpu->regs.pc, 2);

    cpu->regs.pc++;
    uint8_t value = cpu_read(cpu, addr);
    cpu->cycles += 3;
    cpu_read(cpu, addr); // ii+d:1
    cpu->cycles += 1;
    bit_(cpu, value, bit);
    cpu->regs.main.f &= ~MASK_FLAG_XY;
    cpu->regs.main.f |= (cpu->regs.w & MASK_FLAG_XY);
}

static inline uint8_t res(uint8_t value, uint8_t bit)
{
    uint8_t mask = ~(1<<bit);
    return value & mask;
}

static void res_r(Z80_t *cpu, uint8_t *dest, uint8_t bit)
{
    cpu->cycles += 4;
    cpu->regs.pc++;
    *dest = res(*dest, bit);
}

static void res_rra(Z80_t *cpu, uint16_t addr, uint8_t bit)
{
    cpu->cycles += 4;
    cpu->regs.pc++;
    uint8_t value = cpu_read(cpu, addr);
    cpu->cycles += 3;
    cpu_read(cpu, addr); // hl:1
    cpu->cycles += 1;
    value = res(value, bit);
    cpu_write(cpu, addr, value);
    cpu->cycles += 3;
}

static void res_iid_r(Z80_t *cpu, uint16_t addr, uint8_t *dest, uint8_t bit)
{
    cpu->cycles += 3;
    // pc+3:1 x2
    cpu_memory_stall(cpu, cpu->regs.pc, 2);

    cpu->regs.pc++;
    uint8_t value = cpu_read(cpu, addr);
    cpu->cycles += 3;
    cpu_read(cpu, addr); // ii+d:1
    cpu->cycles += 1;
    value = res(value, bit);
    cpu_write(cpu, addr, value);
    cpu->cycles += 3;
    if (dest) *dest = value;
}


static inline uint8_t set(uint8_t value, uint8_t bit)
{
    uint8_t mask = (1<<bit);
    return value | mask;
}

static void set_r(Z80_t *cpu, uint8_t *dest, uint8_t bit)
{
    cpu->cycles += 4;
    cpu->regs.pc++;
    *dest = set(*dest, bit);
}

static void set_rra(Z80_t *cpu, uint16_t addr, uint8_t bit)
{
    cpu->cycles += 4;
    cpu->regs.pc++;
    uint8_t value = cpu_read(cpu, addr);
    cpu->cycles += 3;
    cpu_read(cpu, addr); // hl:1
    cpu->cycles += 1;
    value = set(value, bit);
    cpu_write(cpu, addr, value);
    cpu->cycles += 3;
}

static void set_iid_r(Z80_t *cpu, uint16_t addr, uint8_t *dest, uint8_t bit)
{
    cpu->cycles += 3;
    // pc+3:1 x2
    cpu_memory_stall(cpu, cpu->regs.pc, 2);

    cpu->regs.pc++;
    uint8_t value = cpu_read(cpu, addr);
    cpu->cycles += 3;
    cpu_read(cpu, addr); // ii+d:1
    cpu->cycles += 1;
    value = set(value, bit);
    cpu_write(cpu, addr, value);
    cpu->cycles += 3;
    if (dest) *dest = value;
}



/* Jump Group */

static void jp_cc_nn(Z80_t *cpu, bool cc)
{
    cpu->cycles += 4;
    cpu->regs.pc++;
    uint8_t l = cpu_read(cpu, cpu->regs.pc);
    cpu->cycles += 3;
    cpu->regs.pc++;
    uint8_t h = cpu_read(cpu, cpu->regs.pc);
    cpu->cycles += 3;
    cpu->regs.pc++;
    cpu->regs.memptr = MAKE16(l, h);
    if (cc) {
        cpu->regs.pc = cpu->regs.memptr;
    }
}

static void jr_cc_d(Z80_t *cpu, bool cc)
{
    cpu->cycles += 4;
    cpu->regs.pc++;
    int8_t offset = (int8_t)cpu_read(cpu, cpu->regs.pc);
    cpu->cycles += 3;
    if (cc) {
        cpu->regs.pc += offset;
        cpu->regs.memptr = cpu->regs.pc;
        // pc+1:1 x5
        cpu_memory_stall(cpu, cpu->regs.pc, 5);
    }
    cpu->regs.pc++;
}

static void jp_rr(Z80_t *cpu, uint16_t addr)
{
    cpu->cycles += 4;
    cpu->regs.pc = addr;
    cpu->regs.memptr = cpu->regs.pc;
}

static void djnz_d(Z80_t *cpu)
{
    cpu->cycles += 4;
    cpu_read(cpu, MAKE16(cpu->regs.r, cpu->regs.i)); // ir:1
    cpu->cycles += 1;
    cpu->regs.pc++;
    int8_t offset = (int8_t)cpu_read(cpu, cpu->regs.pc);
    cpu->cycles += 3;
    cpu->regs.main.b--;
    if (cpu->regs.main.b) {
        // pc+1:1 x5
        cpu_memory_stall(cpu, cpu->regs.pc, 5);

        cpu->regs.pc += offset;
        cpu->regs.memptr = cpu->regs.pc;
    }
    cpu->regs.pc++;
}

/* Call and Return Group */

static void call_cc_nn(Z80_t *cpu, bool cc)
{
    cpu->cycles += 4;
    cpu->regs.pc++;
    uint8_t l = cpu_read(cpu, cpu->regs.pc);
    cpu->cycles += 3;
    cpu->regs.pc++;
    uint8_t h = cpu_read(cpu, cpu->regs.pc);
    cpu->cycles += 3;
    cpu->regs.pc++;
    cpu->regs.memptr = MAKE16(l, h);
    if (cc) {
        cpu_read(cpu, cpu->regs.pc); // pc+2:1
        cpu->cycles++;
        cpu->regs.sp--;
        cpu_write(cpu, cpu->regs.sp, HIGH8(cpu->regs.pc));
        cpu->cycles += 3;
        cpu->regs.sp--;
        cpu_write(cpu, cpu->regs.sp, LOW8(cpu->regs.pc));
        cpu->cycles += 3;
        cpu->regs.pc = cpu->regs.memptr;
    }
}

// RET cc (and ONLY cc as timings are different)
static void ret_cc(Z80_t *cpu, bool cc)
{
    cpu->cycles += 4;
    cpu_read(cpu, MAKE16(cpu->regs.r, cpu->regs.i)); // ir:1
    cpu->cycles += 1;
    cpu->regs.pc++;
    if (cc) {
        uint8_t l = cpu_read(cpu, cpu->regs.sp);
        cpu->cycles += 3;
        cpu->regs.sp++;
        uint8_t h = cpu_read(cpu, cpu->regs.sp);
        cpu->cycles += 3;
        cpu->regs.sp++;
        cpu->regs.pc = MAKE16(l, h);
        cpu->regs.memptr = cpu->regs.pc;
    }
}

static void rst(Z80_t *cpu, uint8_t offset)
{
    cpu->cycles += 4;
    cpu_read(cpu, MAKE16(cpu->regs.r, cpu->regs.i)); // ir:1
    cpu->cycles += 1;
    cpu->regs.pc++;
    cpu->regs.sp--;
    cpu_write(cpu, cpu->regs.sp, HIGH8(cpu->regs.pc));
    cpu->cycles += 3;
    cpu->regs.sp--;
    cpu_write(cpu, cpu->regs.sp, LOW8(cpu->regs.pc));
    cpu->cycles += 3;
    cpu->regs.pc = offset;
    cpu->regs.memptr = cpu->regs.pc;
}


/* Input and Output Group */

/* OUT (n), a */
static void out_na_a(Z80_t *cpu)
{
    cpu->cycles += 4;
    cpu->regs.pc++;
    uint8_t l = cpu_read(cpu, cpu->regs.pc);
    cpu->regs.w = cpu->regs.main.a;
    cpu->regs.z = (l + 1) & 0xFF;
    cpu->cycles += 3;
    cpu->regs.pc++;
    uint16_t addr = MAKE16(l, cpu->regs.main.a);
    cpu_out(cpu, addr, cpu->regs.main.a);
    cpu->cycles += 4;
}

static void out_c_r(Z80_t *cpu, uint8_t value)
{
    cpu->cycles += 4;
    cpu->regs.pc++;
    cpu->regs.memptr = cpu->regs.main.bc+1;
    cpu_out(cpu, cpu->regs.main.bc, value);
    cpu->cycles += 4;
}

static void in_r_c(Z80_t *cpu, uint8_t *dest)
{
    cpu->cycles += 4;
    cpu->regs.pc++;
    cpu->regs.memptr = cpu->regs.main.bc+1; // FIXME: dunno about correctness
    uint8_t value = cpu_in(cpu, cpu->regs.main.bc);
    if (dest) *dest = value; 
    cpu->cycles += 4;

    cpu->regs.main.flags.s = value & (1<<7);
    cpu->regs.main.flags.z = !value;
    cpu->regs.main.flags.h = 0;
    cpu->regs.main.flags.pv = get_parity(value);
    cpu->regs.main.flags.n = 0;
    cpu->regs.q = true;
} 

static void in_a_na(Z80_t *cpu)
{
    cpu->cycles += 4;
    cpu->regs.pc++;
    uint8_t l = cpu_read(cpu, cpu->regs.pc);
    cpu->cycles += 3;
    cpu->regs.pc++;
    uint16_t addr = MAKE16(l, cpu->regs.main.a);
    cpu->regs.memptr = addr+1;
    uint8_t value = cpu_in(cpu, addr);
    cpu->regs.main.a = value;
    cpu->cycles += 4;
} 


/* CPU Control Group */

static void nop(Z80_t *cpu)
{
    cpu->cycles += 4;
    cpu->regs.pc++;
}

static void halt(Z80_t *cpu)
{
    cpu->cycles += 4;
    cpu->halted = true;
}

static void di(Z80_t *cpu)
{
    cpu->regs.iff1 = 0;
    cpu->regs.iff2 = 0;
    cpu->cycles += 4;
    cpu->regs.pc++;
}

static void ei(Z80_t *cpu)
{
    cpu->regs.iff1 = 1;
    cpu->regs.iff2 = 1;
    cpu->cycles += 4;
    cpu->regs.pc++;

    cpu->last_ei = true;
}

/* DD/FD handler */

static void ddfd(Z80_t *cpu, bool is_iy)
{
    cpu->cycles += 4;
    cpu->regs.pc++;
    cpu->prefix_state = is_iy ? STATE_FD : STATE_DD;
}

/* The Real Deal */

void cpu_init(Z80_t *cpu)
{
    cpu->cycles = 0;
    cpu->prefix_state = STATE_NOPREFIX;

    cpu->regs.main.af = 0xFFFF;
    cpu->regs.main.bc = 0xFFFF;
    cpu->regs.main.de = 0xFFFF;
    cpu->regs.main.hl = 0xFFFF;

    cpu->regs.alt = cpu->regs.main;

    cpu->regs.sp = 0xFFFF;
    cpu->regs.i = 0xFF;
    cpu->regs.r = 0xFF;

    cpu->regs.iff1 = 0;
    cpu->regs.iff2 = 0;
    cpu->regs.q = 0;
    cpu->regs.im = 0;
    cpu->regs.pc = 0;

    cpu->error = 0;
    cpu->halted = 0;
    cpu->last_ei = 0;
    cpu->interrupt_pending = false;
}

static void do_ed(Z80_t *cpu)
{
    cpu->cycles += 4;
    cpu->regs.pc++;
    uint8_t op = cpu_read(cpu, cpu->regs.pc);
    inc_refresh(cpu);

    switch (op)
    {
    // sbc hl, rr
    case 0x42: sbc_rr_rr(cpu, &cpu->regs.main.hl, cpu->regs.main.bc); break;
    case 0x52: sbc_rr_rr(cpu, &cpu->regs.main.hl, cpu->regs.main.de); break;
    case 0x62: sbc_rr_rr(cpu, &cpu->regs.main.hl, cpu->regs.main.hl); break;
    case 0x72: sbc_rr_rr(cpu, &cpu->regs.main.hl, cpu->regs.sp); break;
    // ld (nn), rr
    case 0x43: ld_nna_rr(cpu, cpu->regs.main.bc); break;
    case 0x53: ld_nna_rr(cpu, cpu->regs.main.de); break;
    case 0x63: ld_nna_rr(cpu, cpu->regs.main.hl); break;
    case 0x73: ld_nna_rr(cpu, cpu->regs.sp); break;
    // retn/reti
    case 0x45: retn(cpu); break;
    case 0x4D: reti(cpu); break;
    case 0x55: retn(cpu); break;
    case 0x5D: retn(cpu); break;
    case 0x65: retn(cpu); break;
    case 0x6D: retn(cpu); break;
    case 0x75: retn(cpu); break;
    case 0x7D: retn(cpu); break;
    // im 0/1/2
    case 0x46: im(cpu, 0); break;
    case 0x4E: im(cpu, 0); break;
    case 0x56: im(cpu, 1); break;
    case 0x5E: im(cpu, 2); break;
    case 0x66: im(cpu, 0); break;
    case 0x6E: im(cpu, 0); break;
    case 0x76: im(cpu, 1); break;
    case 0x7E: im(cpu, 2); break;
    // adc hl, rr
    case 0x4A: adc_rr_rr(cpu, &cpu->regs.main.hl, cpu->regs.main.bc); break;
    case 0x5A: adc_rr_rr(cpu, &cpu->regs.main.hl, cpu->regs.main.de); break;
    case 0x6A: adc_rr_rr(cpu, &cpu->regs.main.hl, cpu->regs.main.hl); break;
    case 0x7A: adc_rr_rr(cpu, &cpu->regs.main.hl, cpu->regs.sp); break;
    // ld rr, (nn)
    case 0x4B: ld_rr_nna(cpu, &cpu->regs.main.bc); break;
    case 0x5B: ld_rr_nna(cpu, &cpu->regs.main.de); break;
    case 0x6B: ld_rr_nna(cpu, &cpu->regs.main.hl); break;
    case 0x7B: ld_rr_nna(cpu, &cpu->regs.sp); break;

    // ldx/ldxr
    case 0xA0: ldx(cpu,  1); break;
    case 0xA8: ldx(cpu, -1); break;
    case 0xB0: ldxr(cpu,  1); break;
    case 0xB8: ldxr(cpu, -1); break;
    // cpx/cpxr
    case 0xA1: cpx(cpu,  1); break;
    case 0xA9: cpx(cpu, -1); break;
    case 0xB1: cpxr(cpu,  1); break;
    case 0xB9: cpxr(cpu, -1); break;
    // inx/inxr
    case 0xA2: inx(cpu,  1); break;
    case 0xAA: inx(cpu, -1); break;
    case 0xB2: inxr(cpu,  1); break;
    case 0xBA: inxr(cpu, -1); break;
    // cpx/cpxr
    case 0xA3: outx(cpu,  1); break;
    case 0xAB: outx(cpu, -1); break;
    case 0xB3: otxr(cpu,  1); break;
    case 0xBB: otxr(cpu, -1); break;

    // in r, (c)
    case 0x40: in_r_c(cpu, &cpu->regs.main.b); break;
    case 0x48: in_r_c(cpu, &cpu->regs.main.c); break;
    case 0x50: in_r_c(cpu, &cpu->regs.main.d); break;
    case 0x58: in_r_c(cpu, &cpu->regs.main.e); break;
    case 0x60: in_r_c(cpu, &cpu->regs.main.h); break;
    case 0x68: in_r_c(cpu, &cpu->regs.main.l); break;
    case 0x70: in_r_c(cpu, NULL); break;
    case 0x78: in_r_c(cpu, &cpu->regs.main.a); break;

    // out (c), r
    case 0x41: out_c_r(cpu, cpu->regs.main.b); break;
    case 0x49: out_c_r(cpu, cpu->regs.main.c); break;
    case 0x51: out_c_r(cpu, cpu->regs.main.d); break;
    case 0x59: out_c_r(cpu, cpu->regs.main.e); break;
    case 0x61: out_c_r(cpu, cpu->regs.main.h); break;
    case 0x69: out_c_r(cpu, cpu->regs.main.l); break;
    case 0x71: out_c_r(cpu, 0); break;
    case 0x79: out_c_r(cpu, cpu->regs.main.a); break;

    // neg
    case 0x44: neg(cpu); break;
    case 0x4C: neg(cpu); break;
    case 0x54: neg(cpu); break;
    case 0x5C: neg(cpu); break;
    case 0x64: neg(cpu); break;
    case 0x6C: neg(cpu); break;
    case 0x74: neg(cpu); break;
    case 0x7C: neg(cpu); break;

    // ir registers
    case 0x47: ld_i_a(cpu, &cpu->regs.i); break;
    case 0x4F: ld_i_a(cpu, &cpu->regs.r); break;
    case 0x57: ld_a_i(cpu, cpu->regs.i); break;
    case 0x5F: ld_a_i(cpu, cpu->regs.r); break;

    // rld/rrd
    case 0x67: rrd(cpu); break;
    case 0x6F: rld(cpu); break;

    default: nop(cpu); break;
    }
}

/* For bit instructions, I've decided to handle partial decoding 
 * for simplicity, as the opcode table is quite orthogonal. */
static void do_cb(Z80_t *cpu)
{
    uint8_t *regs[] = {
        &cpu->regs.main.b,
        &cpu->regs.main.c,
        &cpu->regs.main.d,
        &cpu->regs.main.e,
        &cpu->regs.main.h,
        &cpu->regs.main.l,
        NULL, // (hl) gets special treatment
        &cpu->regs.main.a,
    };

    cpu->cycles += 4;
    cpu->regs.pc++;
    uint8_t op = cpu_read(cpu, cpu->regs.pc);
    inc_refresh(cpu);

    uint8_t reg = op & 7; // op & 0b111
    uint8_t op_partial = op >> 6;
    uint8_t bit_type = (op >> 3) & 7;

    if (reg == 6) { // (hl)
        switch (op_partial) 
        {
            case 0x00: sro_rra(cpu, cpu->regs.main.hl, bit_type); break;
            case 0x01: bit_rra(cpu, cpu->regs.main.hl, bit_type); break;
            case 0x02: res_rra(cpu, cpu->regs.main.hl, bit_type); break;
            case 0x03: set_rra(cpu, cpu->regs.main.hl, bit_type); break;
        }
    } else { // bcdehla
        uint8_t *regptr = regs[reg];

        switch (op_partial) 
        {
            case 0x00: sro_r(cpu, regptr,  bit_type); break;
            case 0x01: bit_r(cpu, *regptr, bit_type); break;
            case 0x02: res_r(cpu, regptr,  bit_type); break;
            case 0x03: set_r(cpu, regptr,  bit_type); break;
        }
    }
}

static void do_ddfd_cb(Z80_t *cpu, uint16_t *ii)
{
    // as a side effect of how Z80 works internally, all the instructions
    // copy their result to a register specified in the last 3 bits
    uint8_t *regs[] = {
        &cpu->regs.main.b,
        &cpu->regs.main.c,
        &cpu->regs.main.d,
        &cpu->regs.main.e,
        &cpu->regs.main.h,
        &cpu->regs.main.l,
        NULL, // no write
        &cpu->regs.main.a,
    };

    // the offset is always after the CB opcode so I just read it here
    cpu->cycles += 4;
    cpu->regs.pc++;
    int8_t d = (int8_t)cpu_read(cpu, cpu->regs.pc);
    uint16_t addr = *ii + d;
    cpu->regs.memptr = addr; 

    cpu->cycles += 3;
    cpu->regs.pc++;
    uint8_t op = cpu_read(cpu, cpu->regs.pc);

    uint8_t reg = op & 7; // op & 0b111
    uint8_t *regptr = regs[reg];

    uint8_t op_partial = op >> 6;
    uint8_t bit_type = (op >> 3) & 7;

    switch (op_partial)
    {
        case 0x00: sro_iid_r(cpu, addr, regptr, bit_type); break;
        case 0x01: bit_iid(cpu, addr, bit_type); break;
        case 0x02: res_iid_r(cpu, addr, regptr, bit_type); break;
        case 0x03: set_iid_r(cpu, addr, regptr, bit_type); break;
    }
}

/* FD/DD are basically "instructions" that tell the CPU 
 * "use IY/IX instead of HL for the next instruction".
 * there's some funky behavior associated with that i Do Not wanna emulate rn.
 * i also don't bother implementing illegal/duplicate opcodes */
static void do_ddfd(Z80_t *cpu, bool is_iy)
{
    cpu->prefix_state = STATE_NOPREFIX;

    uint16_t *ii = is_iy ? &cpu->regs.iy : &cpu->regs.ix;
    uint8_t *il = (uint8_t *)ii;
    uint8_t *ih = il+1;

    uint8_t op = cpu_read(cpu, cpu->regs.pc);
    inc_refresh(cpu);

    switch (op)
    {
    // ld ii, nn
    case 0x21: ld_rr_nn(cpu, ii); break;
    // inc ii
    case 0x23: inc_rr(cpu, ii); break;
    // dec ii
    case 0x2B: dec_rr(cpu, ii); break;
    // add ii, rr
    case 0x09: add_rr_rr(cpu, ii, cpu->regs.main.bc); break;
    case 0x19: add_rr_rr(cpu, ii, cpu->regs.main.de); break;
    case 0x29: add_rr_rr(cpu, ii, *ii); break;
    case 0x39: add_rr_rr(cpu, ii, cpu->regs.sp); break;

    // inc (ii+d)
    case 0x34: inc_iid(cpu, *ii); break;
    // dec (ii+d)
    case 0x35: dec_iid(cpu, *ii); break;
    // ld (ii+d), n
    case 0x36: ld_iid_n(cpu, *ii); break;

    // ld (nn), ii
    case 0x22: ld_nna_rr(cpu, *ii); break;
    // ld ii, (nn)
    case 0x2A: ld_rr_nna(cpu, ii); break;

    // ld (ii+d), r
    case 0x70: ld_iid_r(cpu, *ii, cpu->regs.main.b); break;
    case 0x71: ld_iid_r(cpu, *ii, cpu->regs.main.c); break;
    case 0x72: ld_iid_r(cpu, *ii, cpu->regs.main.d); break;
    case 0x73: ld_iid_r(cpu, *ii, cpu->regs.main.e); break;
    case 0x74: ld_iid_r(cpu, *ii, cpu->regs.main.h); break;
    case 0x75: ld_iid_r(cpu, *ii, cpu->regs.main.l); break;
    case 0x77: ld_iid_r(cpu, *ii, cpu->regs.main.a); break;

    // ld r, (ii+d)
    case 0x46: ld_r_iid(cpu, &cpu->regs.main.b, *ii); break;
    case 0x4E: ld_r_iid(cpu, &cpu->regs.main.c, *ii); break;
    case 0x56: ld_r_iid(cpu, &cpu->regs.main.d, *ii); break;
    case 0x5E: ld_r_iid(cpu, &cpu->regs.main.e, *ii); break;
    case 0x66: ld_r_iid(cpu, &cpu->regs.main.h, *ii); break;
    case 0x6E: ld_r_iid(cpu, &cpu->regs.main.l, *ii); break;
    case 0x7E: ld_r_iid(cpu, &cpu->regs.main.a, *ii); break;

    // add/adc/sub/sbc/and/xor/or/cp (ii+d)
    case 0x86: alo_iid(cpu, *ii, 0); break;
    case 0x8E: alo_iid(cpu, *ii, 1); break;
    case 0x96: alo_iid(cpu, *ii, 2); break;
    case 0x9E: alo_iid(cpu, *ii, 3); break;
    case 0xA6: alo_iid(cpu, *ii, 4); break;
    case 0xAE: alo_iid(cpu, *ii, 5); break;
    case 0xB6: alo_iid(cpu, *ii, 6); break;
    case 0xBE: alo_iid(cpu, *ii, 7); break;

    // non (ii+d) alo
    // add a
    case 0x80: alo_r(cpu, cpu->regs.main.b, 0); break;
    case 0x81: alo_r(cpu, cpu->regs.main.c, 0); break;
    case 0x82: alo_r(cpu, cpu->regs.main.d, 0); break;
    case 0x83: alo_r(cpu, cpu->regs.main.e, 0); break;
    case 0x84: alo_r(cpu, *ih, 0); break;
    case 0x85: alo_r(cpu, *il, 0); break;
    case 0x87: alo_r(cpu, cpu->regs.main.a, 0); break;
    // adc a
    case 0x88: alo_r(cpu, cpu->regs.main.b, 1); break;
    case 0x89: alo_r(cpu, cpu->regs.main.c, 1); break;
    case 0x8A: alo_r(cpu, cpu->regs.main.d, 1); break;
    case 0x8B: alo_r(cpu, cpu->regs.main.e, 1); break;
    case 0x8C: alo_r(cpu, *ih, 1); break;
    case 0x8D: alo_r(cpu, *il, 1); break;
    case 0x8F: alo_r(cpu, cpu->regs.main.a, 1); break;
    // sub
    case 0x90: alo_r(cpu, cpu->regs.main.b, 2); break;
    case 0x91: alo_r(cpu, cpu->regs.main.c, 2); break;
    case 0x92: alo_r(cpu, cpu->regs.main.d, 2); break;
    case 0x93: alo_r(cpu, cpu->regs.main.e, 2); break;
    case 0x94: alo_r(cpu, *ih, 2); break;
    case 0x95: alo_r(cpu, *il, 2); break;
    case 0x97: alo_r(cpu, cpu->regs.main.a, 2); break;
    // sbc a
    case 0x98: alo_r(cpu, cpu->regs.main.b, 3); break;
    case 0x99: alo_r(cpu, cpu->regs.main.c, 3); break;
    case 0x9A: alo_r(cpu, cpu->regs.main.d, 3); break;
    case 0x9B: alo_r(cpu, cpu->regs.main.e, 3); break;
    case 0x9C: alo_r(cpu, *ih, 3); break;
    case 0x9D: alo_r(cpu, *il, 3); break;
    case 0x9F: alo_r(cpu, cpu->regs.main.a, 3); break;
    // and
    case 0xA0: alo_r(cpu, cpu->regs.main.b, 4); break;
    case 0xA1: alo_r(cpu, cpu->regs.main.c, 4); break;
    case 0xA2: alo_r(cpu, cpu->regs.main.d, 4); break;
    case 0xA3: alo_r(cpu, cpu->regs.main.e, 4); break;
    case 0xA4: alo_r(cpu, *ih, 4); break;
    case 0xA5: alo_r(cpu, *il, 4); break;
    case 0xA7: alo_r(cpu, cpu->regs.main.a, 4); break;
    // xor
    case 0xA8: alo_r(cpu, cpu->regs.main.b, 5); break;
    case 0xA9: alo_r(cpu, cpu->regs.main.c, 5); break;
    case 0xAA: alo_r(cpu, cpu->regs.main.d, 5); break;
    case 0xAB: alo_r(cpu, cpu->regs.main.e, 5); break;
    case 0xAC: alo_r(cpu, *ih, 5); break;
    case 0xAD: alo_r(cpu, *il, 5); break;
    case 0xAF: alo_r(cpu, cpu->regs.main.a, 5); break;
    // or
    case 0xB0: alo_r(cpu, cpu->regs.main.b, 6); break;
    case 0xB1: alo_r(cpu, cpu->regs.main.c, 6); break;
    case 0xB2: alo_r(cpu, cpu->regs.main.d, 6); break;
    case 0xB3: alo_r(cpu, cpu->regs.main.e, 6); break;
    case 0xB4: alo_r(cpu, *ih, 6); break;
    case 0xB5: alo_r(cpu, *il, 6); break;
    case 0xB7: alo_r(cpu, cpu->regs.main.a, 6); break;
    // cp
    case 0xB8: alo_r(cpu, cpu->regs.main.b, 7); break;
    case 0xB9: alo_r(cpu, cpu->regs.main.c, 7); break;
    case 0xBA: alo_r(cpu, cpu->regs.main.d, 7); break;
    case 0xBB: alo_r(cpu, cpu->regs.main.e, 7); break;
    case 0xBC: alo_r(cpu, *ih, 7); break;
    case 0xBD: alo_r(cpu, *il, 7); break;
    case 0xBF: alo_r(cpu, cpu->regs.main.a, 7); break;

    // inc r
    case 0x04: inc_r(cpu, &cpu->regs.main.b); break;
    case 0x0C: inc_r(cpu, &cpu->regs.main.c); break;
    case 0x14: inc_r(cpu, &cpu->regs.main.d); break;
    case 0x1C: inc_r(cpu, &cpu->regs.main.e); break;
    case 0x24: inc_r(cpu, ih); break;
    case 0x2C: inc_r(cpu, il); break;
    case 0x3C: inc_r(cpu, &cpu->regs.main.a); break;
    // dec r
    case 0x05: dec_r(cpu, &cpu->regs.main.b); break;
    case 0x0D: dec_r(cpu, &cpu->regs.main.c); break;
    case 0x15: dec_r(cpu, &cpu->regs.main.d); break;
    case 0x1D: dec_r(cpu, &cpu->regs.main.e); break;
    case 0x25: dec_r(cpu, ih); break;
    case 0x2D: dec_r(cpu, il); break;
    case 0x3D: dec_r(cpu, &cpu->regs.main.a); break;

    // ld b, r
    case 0x40: ld_r_r(cpu, &cpu->regs.main.b, cpu->regs.main.b); break;
    case 0x41: ld_r_r(cpu, &cpu->regs.main.b, cpu->regs.main.c); break;
    case 0x42: ld_r_r(cpu, &cpu->regs.main.b, cpu->regs.main.d); break;
    case 0x43: ld_r_r(cpu, &cpu->regs.main.b, cpu->regs.main.e); break;
    case 0x44: ld_r_r(cpu, &cpu->regs.main.b, *ih); break;
    case 0x45: ld_r_r(cpu, &cpu->regs.main.b, *il); break;
    case 0x47: ld_r_r(cpu, &cpu->regs.main.b, cpu->regs.main.a); break;
    // ld c, r
    case 0x48: ld_r_r(cpu, &cpu->regs.main.c, cpu->regs.main.b); break;
    case 0x49: ld_r_r(cpu, &cpu->regs.main.c, cpu->regs.main.c); break;
    case 0x4A: ld_r_r(cpu, &cpu->regs.main.c, cpu->regs.main.d); break;
    case 0x4B: ld_r_r(cpu, &cpu->regs.main.c, cpu->regs.main.e); break;
    case 0x4C: ld_r_r(cpu, &cpu->regs.main.c, *ih); break;
    case 0x4D: ld_r_r(cpu, &cpu->regs.main.c, *il); break;
    case 0x4F: ld_r_r(cpu, &cpu->regs.main.c, cpu->regs.main.a); break;
    // ld d, r
    case 0x50: ld_r_r(cpu, &cpu->regs.main.d, cpu->regs.main.b); break;
    case 0x51: ld_r_r(cpu, &cpu->regs.main.d, cpu->regs.main.c); break;
    case 0x52: ld_r_r(cpu, &cpu->regs.main.d, cpu->regs.main.d); break;
    case 0x53: ld_r_r(cpu, &cpu->regs.main.d, cpu->regs.main.e); break;
    case 0x54: ld_r_r(cpu, &cpu->regs.main.d, *ih); break;
    case 0x55: ld_r_r(cpu, &cpu->regs.main.d, *il); break;
    case 0x57: ld_r_r(cpu, &cpu->regs.main.d, cpu->regs.main.a); break;
    // ld e, r
    case 0x58: ld_r_r(cpu, &cpu->regs.main.e, cpu->regs.main.b); break;
    case 0x59: ld_r_r(cpu, &cpu->regs.main.e, cpu->regs.main.c); break;
    case 0x5A: ld_r_r(cpu, &cpu->regs.main.e, cpu->regs.main.d); break;
    case 0x5B: ld_r_r(cpu, &cpu->regs.main.e, cpu->regs.main.e); break;
    case 0x5C: ld_r_r(cpu, &cpu->regs.main.e, *ih); break;
    case 0x5D: ld_r_r(cpu, &cpu->regs.main.e, *il); break;
    case 0x5F: ld_r_r(cpu, &cpu->regs.main.e, cpu->regs.main.a); break;
    // ld ih, r
    case 0x60: ld_r_r(cpu, ih, cpu->regs.main.b); break;
    case 0x61: ld_r_r(cpu, ih, cpu->regs.main.c); break;
    case 0x62: ld_r_r(cpu, ih, cpu->regs.main.d); break;
    case 0x63: ld_r_r(cpu, ih, cpu->regs.main.e); break;
    case 0x64: ld_r_r(cpu, ih, *ih); break;
    case 0x65: ld_r_r(cpu, ih, *il); break;
    case 0x67: ld_r_r(cpu, ih, cpu->regs.main.a); break;
    // ld il, r
    case 0x68: ld_r_r(cpu, il, cpu->regs.main.b); break;
    case 0x69: ld_r_r(cpu, il, cpu->regs.main.c); break;
    case 0x6A: ld_r_r(cpu, il, cpu->regs.main.d); break;
    case 0x6B: ld_r_r(cpu, il, cpu->regs.main.e); break;
    case 0x6C: ld_r_r(cpu, il, *ih); break;
    case 0x6D: ld_r_r(cpu, il, *il); break;
    case 0x6F: ld_r_r(cpu, il, cpu->regs.main.a); break;
    // ld a, r
    case 0x78: ld_r_r(cpu, &cpu->regs.main.a, cpu->regs.main.b); break;
    case 0x79: ld_r_r(cpu, &cpu->regs.main.a, cpu->regs.main.c); break;
    case 0x7A: ld_r_r(cpu, &cpu->regs.main.a, cpu->regs.main.d); break;
    case 0x7B: ld_r_r(cpu, &cpu->regs.main.a, cpu->regs.main.e); break;
    case 0x7C: ld_r_r(cpu, &cpu->regs.main.a, *ih); break;
    case 0x7D: ld_r_r(cpu, &cpu->regs.main.a, *il); break;
    case 0x7F: ld_r_r(cpu, &cpu->regs.main.a, cpu->regs.main.a); break;

    // ld r, n
    case 0x06: ld_r_n(cpu, &cpu->regs.main.b); break;
    case 0x0E: ld_r_n(cpu, &cpu->regs.main.c); break;
    case 0x16: ld_r_n(cpu, &cpu->regs.main.d); break;
    case 0x1E: ld_r_n(cpu, &cpu->regs.main.e); break;
    case 0x26: ld_r_n(cpu, ih); break;
    case 0x2E: ld_r_n(cpu, il); break;
    case 0x3E: ld_r_n(cpu, &cpu->regs.main.a); break;

    // ld sp, ii
    case 0xF9: ld_sp_rr(cpu, *ii); break;

    // jp ii
    case 0xE9: jp_rr(cpu, *ii); break;

    // IY/IX prefix
    case 0xDD: ddfd(cpu, false); break;
    case 0xFD: ddfd(cpu, true); break;

    // pop/push ii
    case 0xE1: pop(cpu, ii); break;
    case 0xE5: push(cpu, *ii); break;
    // ex (sp), ii
    case 0xE3: ex_spa_rr(cpu, ii); break;

    // bit instructions
    case 0xCB: do_ddfd_cb(cpu, ii); break;

    // DUPLICATE OPCODES
    // ld rr, nn
    case 0x01: ld_rr_nn(cpu, &cpu->regs.main.bc); break;
    case 0x11: ld_rr_nn(cpu, &cpu->regs.main.de); break;
    case 0x31: ld_rr_nn(cpu, &cpu->regs.sp); break;

    // ld (rr), a
    case 0x02: ld_rra_a(cpu, cpu->regs.main.bc); break;
    case 0x12: ld_rra_a(cpu, cpu->regs.main.de); break;
    // ld a, (rr)
    case 0x0A: ld_a_rra(cpu, cpu->regs.main.bc); break;
    case 0x1A: ld_a_rra(cpu, cpu->regs.main.de); break;
    // ld a, (nn)
    case 0x3A: ld_a_nna(cpu); break;
    // ld (nn), a
    case 0x32: ld_nna_a(cpu); break;
    // inc rr
    case 0x03: inc_rr(cpu, &cpu->regs.main.bc); break;
    case 0x13: inc_rr(cpu, &cpu->regs.main.de); break;
    case 0x33: inc_rr(cpu, &cpu->regs.sp); break;
    // dec rr
    case 0x0B: dec_rr(cpu, &cpu->regs.main.bc); break;
    case 0x1B: dec_rr(cpu, &cpu->regs.main.de); break;
    case 0x3B: dec_rr(cpu, &cpu->regs.sp); break;

    // rra/rla/rrca/rlca
    case 0x07: rlca(cpu); break;
    case 0x0F: rrca(cpu); break;
    case 0x17: rla(cpu); break;
    case 0x1F: rra(cpu); break;

    case 0x27: daa(cpu); break;
    // cpl
    case 0x2F: cpl(cpu); break;
    // scf/ccf
    case 0x37: scf(cpu); break;
    case 0x3F: ccf(cpu); break;

    // add/adc/sub/sbc/and/xor/or/cp n
    case 0xC6: alo_n(cpu, 0); break;
    case 0xCE: alo_n(cpu, 1); break;
    case 0xD6: alo_n(cpu, 2); break;
    case 0xDE: alo_n(cpu, 3); break;
    case 0xE6: alo_n(cpu, 4); break;
    case 0xEE: alo_n(cpu, 5); break;
    case 0xF6: alo_n(cpu, 6); break;
    case 0xFE: alo_n(cpu, 7); break;

    // djnz
    case 0x10: djnz_d(cpu); break;
    // jr
    case 0x18: jr_cc_d(cpu, true); break;
    // jr cc, d
    case 0x20: jr_cc_d(cpu, !cpu->regs.main.flags.z); break;
    case 0x28: jr_cc_d(cpu, cpu->regs.main.flags.z); break;
    case 0x30: jr_cc_d(cpu, !cpu->regs.main.flags.c); break;
    case 0x38: jr_cc_d(cpu, cpu->regs.main.flags.c); break;
    // jp cc, nn
    case 0xC2: jp_cc_nn(cpu, !cpu->regs.main.flags.z); break;
    case 0xCA: jp_cc_nn(cpu, cpu->regs.main.flags.z); break;
    case 0xD2: jp_cc_nn(cpu, !cpu->regs.main.flags.c); break;
    case 0xDA: jp_cc_nn(cpu, cpu->regs.main.flags.c); break;
    case 0xE2: jp_cc_nn(cpu, !cpu->regs.main.flags.pv); break;
    case 0xEA: jp_cc_nn(cpu, cpu->regs.main.flags.pv); break;
    case 0xF2: jp_cc_nn(cpu, !cpu->regs.main.flags.s); break;
    case 0xFA: jp_cc_nn(cpu, cpu->regs.main.flags.s); break;
    // jp nn
    case 0xC3: jp_cc_nn(cpu, true); break;
    // call cc, nn
    case 0xC4: call_cc_nn(cpu, !cpu->regs.main.flags.z); break;
    case 0xCC: call_cc_nn(cpu, cpu->regs.main.flags.z); break;
    case 0xD4: call_cc_nn(cpu, !cpu->regs.main.flags.c); break;
    case 0xDC: call_cc_nn(cpu, cpu->regs.main.flags.c); break;
    case 0xE4: call_cc_nn(cpu, !cpu->regs.main.flags.pv); break;
    case 0xEC: call_cc_nn(cpu, cpu->regs.main.flags.pv); break;
    case 0xF4: call_cc_nn(cpu, !cpu->regs.main.flags.s); break;
    case 0xFC: call_cc_nn(cpu, cpu->regs.main.flags.s); break;
    // call nn
    case 0xCD: call_cc_nn(cpu, true); break;
    // ret cc
    case 0xC0: ret_cc(cpu, !cpu->regs.main.flags.z); break;
    case 0xC8: ret_cc(cpu, cpu->regs.main.flags.z); break;
    case 0xD0: ret_cc(cpu, !cpu->regs.main.flags.c); break;
    case 0xD8: ret_cc(cpu, cpu->regs.main.flags.c); break;
    case 0xE0: ret_cc(cpu, !cpu->regs.main.flags.pv); break;
    case 0xE8: ret_cc(cpu, cpu->regs.main.flags.pv); break;
    case 0xF0: ret_cc(cpu, !cpu->regs.main.flags.s); break;
    case 0xF8: ret_cc(cpu, cpu->regs.main.flags.s); break;
    // ret (same behavior as imaginary instruction pop pc)
    case 0xC9: ret(cpu); break;

    // rst
    case 0xC7: rst(cpu, 0x00); break;
    case 0xCF: rst(cpu, 0x08); break;
    case 0xD7: rst(cpu, 0x10); break;
    case 0xDF: rst(cpu, 0x18); break;
    case 0xE7: rst(cpu, 0x20); break;
    case 0xEF: rst(cpu, 0x28); break;
    case 0xF7: rst(cpu, 0x30); break;
    case 0xFF: rst(cpu, 0x38); break;

    // pop
    case 0xC1: pop(cpu, &cpu->regs.main.bc); break;
    case 0xD1: pop(cpu, &cpu->regs.main.de); break;
    case 0xF1: pop(cpu, &cpu->regs.main.af); break;
    // push
    case 0xC5: push(cpu, cpu->regs.main.bc); break;
    case 0xD5: push(cpu, cpu->regs.main.de); break;
    case 0xF5: push(cpu, cpu->regs.main.af); break;

    // out (n), a
    case 0xD3: out_na_a(cpu); break;
    // in a, (n)
    case 0xDB: in_a_na(cpu); break;

    // ex af
    case 0x08: ex_af(cpu); break;
    // exx
    case 0xD9: exx(cpu); break;
    // XD
    case 0xEB: ex_de_hl(cpu); break;

    // DI / EI
    case 0xF3: di(cpu); break;
    case 0xFB: ei(cpu); break;
    // halt
    case 0x76: halt(cpu); break;

    case 0x00: nop(cpu); break;

    // misc instructions
    case 0xED: do_ed(cpu); break;

    default:
        print_regs(cpu);

        static const char ix_op[] = "DD";
        static const char iy_op[] = "FD";
        const char *prefix = is_iy ? iy_op : ix_op; 

        dlog(LOG_ERR, "unimplemented opcode %s %02X at %04X", prefix, op, cpu->regs.pc);
        cpu->error = 1;
    }
}

static void do_opcode(Z80_t *cpu)
{
    // oppa gangnam style
    uint8_t op = cpu_read(cpu, cpu->regs.pc);
    inc_refresh(cpu);

    switch (op)
    {
    // ld rr, nn
    case 0x01: ld_rr_nn(cpu, &cpu->regs.main.bc); break;
    case 0x11: ld_rr_nn(cpu, &cpu->regs.main.de); break;
    case 0x21: ld_rr_nn(cpu, &cpu->regs.main.hl); break;
    case 0x31: ld_rr_nn(cpu, &cpu->regs.sp); break;

    // ld (rr), a
    case 0x02: ld_rra_a(cpu, cpu->regs.main.bc); break;
    case 0x12: ld_rra_a(cpu, cpu->regs.main.de); break;
    // ld a, (rr)
    case 0x0A: ld_a_rra(cpu, cpu->regs.main.bc); break;
    case 0x1A: ld_a_rra(cpu, cpu->regs.main.de); break;
    // ld a, (nn)
    case 0x3A: ld_a_nna(cpu); break;

    // inc rr
    case 0x03: inc_rr(cpu, &cpu->regs.main.bc); break;
    case 0x13: inc_rr(cpu, &cpu->regs.main.de); break;
    case 0x23: inc_rr(cpu, &cpu->regs.main.hl); break;
    case 0x33: inc_rr(cpu, &cpu->regs.sp); break;
    // inc r
    case 0x04: inc_r(cpu, &cpu->regs.main.b); break;
    case 0x0C: inc_r(cpu, &cpu->regs.main.c); break;
    case 0x14: inc_r(cpu, &cpu->regs.main.d); break;
    case 0x1C: inc_r(cpu, &cpu->regs.main.e); break;
    case 0x24: inc_r(cpu, &cpu->regs.main.h); break;
    case 0x2C: inc_r(cpu, &cpu->regs.main.l); break;
    case 0x34: inc_rra(cpu, cpu->regs.main.hl); break;
    case 0x3C: inc_r(cpu, &cpu->regs.main.a); break;
    // dec r
    case 0x05: dec_r(cpu, &cpu->regs.main.b); break;
    case 0x0D: dec_r(cpu, &cpu->regs.main.c); break;
    case 0x15: dec_r(cpu, &cpu->regs.main.d); break;
    case 0x1D: dec_r(cpu, &cpu->regs.main.e); break;
    case 0x25: dec_r(cpu, &cpu->regs.main.h); break;
    case 0x2D: dec_r(cpu, &cpu->regs.main.l); break;
    case 0x35: dec_rra(cpu, cpu->regs.main.hl); break;
    case 0x3D: dec_r(cpu, &cpu->regs.main.a); break;
    // dec rr
    case 0x0B: dec_rr(cpu, &cpu->regs.main.bc); break;
    case 0x1B: dec_rr(cpu, &cpu->regs.main.de); break;
    case 0x2B: dec_rr(cpu, &cpu->regs.main.hl); break;
    case 0x3B: dec_rr(cpu, &cpu->regs.sp); break;

    // ld r, n
    case 0x06: ld_r_n(cpu, &cpu->regs.main.b); break;
    case 0x0E: ld_r_n(cpu, &cpu->regs.main.c); break;
    case 0x16: ld_r_n(cpu, &cpu->regs.main.d); break;
    case 0x1E: ld_r_n(cpu, &cpu->regs.main.e); break;
    case 0x26: ld_r_n(cpu, &cpu->regs.main.h); break;
    case 0x2E: ld_r_n(cpu, &cpu->regs.main.l); break;
    case 0x36: ld_rra_n(cpu, cpu->regs.main.hl); break;
    case 0x3E: ld_r_n(cpu, &cpu->regs.main.a); break;

    // add hl, rr
    case 0x09: add_rr_rr(cpu, &cpu->regs.main.hl, cpu->regs.main.bc); break;
    case 0x19: add_rr_rr(cpu, &cpu->regs.main.hl, cpu->regs.main.de); break;
    case 0x29: add_rr_rr(cpu, &cpu->regs.main.hl, cpu->regs.main.hl); break;
    case 0x39: add_rr_rr(cpu, &cpu->regs.main.hl, cpu->regs.sp); break;

    // rra/rla/rrca/rlca
    case 0x07: rlca(cpu); break;
    case 0x0F: rrca(cpu); break;
    case 0x17: rla(cpu); break;
    case 0x1F: rra(cpu); break;

    // ld (nn), hl
    case 0x22: ld_nna_rr(cpu, cpu->regs.main.hl); break;
    // ld hl, (nn)
    case 0x2A: ld_rr_nna(cpu, &cpu->regs.main.hl); break;
    // ld (nn), a
    case 0x32: ld_nna_a(cpu); break;
    // ld sp, hl
    case 0xF9: ld_sp_rr(cpu, cpu->regs.main.hl); break;

    case 0x27: daa(cpu); break;
    // cpl
    case 0x2F: cpl(cpu); break;
    // scf/ccf
    case 0x37: scf(cpu); break;
    case 0x3F: ccf(cpu); break;

    // ld b, reg
    case 0x40: ld_r_r(cpu, &cpu->regs.main.b, cpu->regs.main.b); break;
    case 0x41: ld_r_r(cpu, &cpu->regs.main.b, cpu->regs.main.c); break;
    case 0x42: ld_r_r(cpu, &cpu->regs.main.b, cpu->regs.main.d); break;
    case 0x43: ld_r_r(cpu, &cpu->regs.main.b, cpu->regs.main.e); break;
    case 0x44: ld_r_r(cpu, &cpu->regs.main.b, cpu->regs.main.h); break;
    case 0x45: ld_r_r(cpu, &cpu->regs.main.b, cpu->regs.main.l); break;
    case 0x46: ld_r_rra(cpu, &cpu->regs.main.b, cpu->regs.main.hl); break;
    case 0x47: ld_r_r(cpu, &cpu->regs.main.b, cpu->regs.main.a); break;
    // ld c, reg
    case 0x48: ld_r_r(cpu, &cpu->regs.main.c, cpu->regs.main.b); break;
    case 0x49: ld_r_r(cpu, &cpu->regs.main.c, cpu->regs.main.c); break;
    case 0x4A: ld_r_r(cpu, &cpu->regs.main.c, cpu->regs.main.d); break;
    case 0x4B: ld_r_r(cpu, &cpu->regs.main.c, cpu->regs.main.e); break;
    case 0x4C: ld_r_r(cpu, &cpu->regs.main.c, cpu->regs.main.h); break;
    case 0x4D: ld_r_r(cpu, &cpu->regs.main.c, cpu->regs.main.l); break;
    case 0x4E: ld_r_rra(cpu, &cpu->regs.main.c, cpu->regs.main.hl); break;
    case 0x4F: ld_r_r(cpu, &cpu->regs.main.c, cpu->regs.main.a); break;
    // ld d, reg
    case 0x50: ld_r_r(cpu, &cpu->regs.main.d, cpu->regs.main.b); break;
    case 0x51: ld_r_r(cpu, &cpu->regs.main.d, cpu->regs.main.c); break;
    case 0x52: ld_r_r(cpu, &cpu->regs.main.d, cpu->regs.main.d); break;
    case 0x53: ld_r_r(cpu, &cpu->regs.main.d, cpu->regs.main.e); break;
    case 0x54: ld_r_r(cpu, &cpu->regs.main.d, cpu->regs.main.h); break;
    case 0x55: ld_r_r(cpu, &cpu->regs.main.d, cpu->regs.main.l); break;
    case 0x56: ld_r_rra(cpu, &cpu->regs.main.d, cpu->regs.main.hl); break;
    case 0x57: ld_r_r(cpu, &cpu->regs.main.d, cpu->regs.main.a); break;
    // ld e, reg
    case 0x58: ld_r_r(cpu, &cpu->regs.main.e, cpu->regs.main.b); break;
    case 0x59: ld_r_r(cpu, &cpu->regs.main.e, cpu->regs.main.c); break;
    case 0x5A: ld_r_r(cpu, &cpu->regs.main.e, cpu->regs.main.d); break;
    case 0x5B: ld_r_r(cpu, &cpu->regs.main.e, cpu->regs.main.e); break;
    case 0x5C: ld_r_r(cpu, &cpu->regs.main.e, cpu->regs.main.h); break;
    case 0x5D: ld_r_r(cpu, &cpu->regs.main.e, cpu->regs.main.l); break;
    case 0x5E: ld_r_rra(cpu, &cpu->regs.main.e, cpu->regs.main.hl); break;
    case 0x5F: ld_r_r(cpu, &cpu->regs.main.e, cpu->regs.main.a); break;
    // ld h, reg
    case 0x60: ld_r_r(cpu, &cpu->regs.main.h, cpu->regs.main.b); break;
    case 0x61: ld_r_r(cpu, &cpu->regs.main.h, cpu->regs.main.c); break;
    case 0x62: ld_r_r(cpu, &cpu->regs.main.h, cpu->regs.main.d); break;
    case 0x63: ld_r_r(cpu, &cpu->regs.main.h, cpu->regs.main.e); break;
    case 0x64: ld_r_r(cpu, &cpu->regs.main.h, cpu->regs.main.h); break;
    case 0x65: ld_r_r(cpu, &cpu->regs.main.h, cpu->regs.main.l); break;
    case 0x66: ld_r_rra(cpu, &cpu->regs.main.h, cpu->regs.main.hl); break;
    case 0x67: ld_r_r(cpu, &cpu->regs.main.h, cpu->regs.main.a); break;
    // ld l, reg
    case 0x68: ld_r_r(cpu, &cpu->regs.main.l, cpu->regs.main.b); break;
    case 0x69: ld_r_r(cpu, &cpu->regs.main.l, cpu->regs.main.c); break;
    case 0x6A: ld_r_r(cpu, &cpu->regs.main.l, cpu->regs.main.d); break;
    case 0x6B: ld_r_r(cpu, &cpu->regs.main.l, cpu->regs.main.e); break;
    case 0x6C: ld_r_r(cpu, &cpu->regs.main.l, cpu->regs.main.h); break;
    case 0x6D: ld_r_r(cpu, &cpu->regs.main.l, cpu->regs.main.l); break;
    case 0x6E: ld_r_rra(cpu, &cpu->regs.main.l, cpu->regs.main.hl); break;
    case 0x6F: ld_r_r(cpu, &cpu->regs.main.l, cpu->regs.main.a); break;
    // ld a, reg
    case 0x78: ld_r_r(cpu, &cpu->regs.main.a, cpu->regs.main.b); break;
    case 0x79: ld_r_r(cpu, &cpu->regs.main.a, cpu->regs.main.c); break;
    case 0x7A: ld_r_r(cpu, &cpu->regs.main.a, cpu->regs.main.d); break;
    case 0x7B: ld_r_r(cpu, &cpu->regs.main.a, cpu->regs.main.e); break;
    case 0x7C: ld_r_r(cpu, &cpu->regs.main.a, cpu->regs.main.h); break;
    case 0x7D: ld_r_r(cpu, &cpu->regs.main.a, cpu->regs.main.l); break;
    case 0x7E: ld_r_rra(cpu, &cpu->regs.main.a, cpu->regs.main.hl); break;
    case 0x7F: ld_r_r(cpu, &cpu->regs.main.a, cpu->regs.main.a); break;

    // ld (hl), r
    case 0x70: ld_rra_r(cpu, cpu->regs.main.hl, cpu->regs.main.b); break;
    case 0x71: ld_rra_r(cpu, cpu->regs.main.hl, cpu->regs.main.c); break;
    case 0x72: ld_rra_r(cpu, cpu->regs.main.hl, cpu->regs.main.d); break;
    case 0x73: ld_rra_r(cpu, cpu->regs.main.hl, cpu->regs.main.e); break;
    case 0x74: ld_rra_r(cpu, cpu->regs.main.hl, cpu->regs.main.h); break;
    case 0x75: ld_rra_r(cpu, cpu->regs.main.hl, cpu->regs.main.l); break;
    case 0x77: ld_rra_r(cpu, cpu->regs.main.hl, cpu->regs.main.a); break;

    // add a
    case 0x80: alo_r(cpu, cpu->regs.main.b, 0); break;
    case 0x81: alo_r(cpu, cpu->regs.main.c, 0); break;
    case 0x82: alo_r(cpu, cpu->regs.main.d, 0); break;
    case 0x83: alo_r(cpu, cpu->regs.main.e, 0); break;
    case 0x84: alo_r(cpu, cpu->regs.main.h, 0); break;
    case 0x85: alo_r(cpu, cpu->regs.main.l, 0); break;
    case 0x86: alo_rra(cpu, cpu->regs.main.hl, 0); break;
    case 0x87: alo_r(cpu, cpu->regs.main.a, 0); break;
    // adc a
    case 0x88: alo_r(cpu, cpu->regs.main.b, 1); break;
    case 0x89: alo_r(cpu, cpu->regs.main.c, 1); break;
    case 0x8A: alo_r(cpu, cpu->regs.main.d, 1); break;
    case 0x8B: alo_r(cpu, cpu->regs.main.e, 1); break;
    case 0x8C: alo_r(cpu, cpu->regs.main.h, 1); break;
    case 0x8D: alo_r(cpu, cpu->regs.main.l, 1); break;
    case 0x8E: alo_rra(cpu, cpu->regs.main.hl, 1); break;
    case 0x8F: alo_r(cpu, cpu->regs.main.a, 1); break;
    // sub
    case 0x90: alo_r(cpu, cpu->regs.main.b, 2); break;
    case 0x91: alo_r(cpu, cpu->regs.main.c, 2); break;
    case 0x92: alo_r(cpu, cpu->regs.main.d, 2); break;
    case 0x93: alo_r(cpu, cpu->regs.main.e, 2); break;
    case 0x94: alo_r(cpu, cpu->regs.main.h, 2); break;
    case 0x95: alo_r(cpu, cpu->regs.main.l, 2); break;
    case 0x96: alo_rra(cpu, cpu->regs.main.hl, 2); break;
    case 0x97: alo_r(cpu, cpu->regs.main.a, 2); break;
    // sbc a
    case 0x98: alo_r(cpu, cpu->regs.main.b, 3); break;
    case 0x99: alo_r(cpu, cpu->regs.main.c, 3); break;
    case 0x9A: alo_r(cpu, cpu->regs.main.d, 3); break;
    case 0x9B: alo_r(cpu, cpu->regs.main.e, 3); break;
    case 0x9C: alo_r(cpu, cpu->regs.main.h, 3); break;
    case 0x9D: alo_r(cpu, cpu->regs.main.l, 3); break;
    case 0x9E: alo_rra(cpu, cpu->regs.main.hl, 3); break;
    case 0x9F: alo_r(cpu, cpu->regs.main.a, 3); break;
    // and
    case 0xA0: alo_r(cpu, cpu->regs.main.b, 4); break;
    case 0xA1: alo_r(cpu, cpu->regs.main.c, 4); break;
    case 0xA2: alo_r(cpu, cpu->regs.main.d, 4); break;
    case 0xA3: alo_r(cpu, cpu->regs.main.e, 4); break;
    case 0xA4: alo_r(cpu, cpu->regs.main.h, 4); break;
    case 0xA5: alo_r(cpu, cpu->regs.main.l, 4); break;
    case 0xA6: alo_rra(cpu, cpu->regs.main.hl, 4); break;
    case 0xA7: alo_r(cpu, cpu->regs.main.a, 4); break;
    // xor
    case 0xA8: alo_r(cpu, cpu->regs.main.b, 5); break;
    case 0xA9: alo_r(cpu, cpu->regs.main.c, 5); break;
    case 0xAA: alo_r(cpu, cpu->regs.main.d, 5); break;
    case 0xAB: alo_r(cpu, cpu->regs.main.e, 5); break;
    case 0xAC: alo_r(cpu, cpu->regs.main.h, 5); break;
    case 0xAD: alo_r(cpu, cpu->regs.main.l, 5); break;
    case 0xAE: alo_rra(cpu, cpu->regs.main.hl, 5); break;
    case 0xAF: alo_r(cpu, cpu->regs.main.a, 5); break;
    // or
    case 0xB0: alo_r(cpu, cpu->regs.main.b, 6); break;
    case 0xB1: alo_r(cpu, cpu->regs.main.c, 6); break;
    case 0xB2: alo_r(cpu, cpu->regs.main.d, 6); break;
    case 0xB3: alo_r(cpu, cpu->regs.main.e, 6); break;
    case 0xB4: alo_r(cpu, cpu->regs.main.h, 6); break;
    case 0xB5: alo_r(cpu, cpu->regs.main.l, 6); break;
    case 0xB6: alo_rra(cpu, cpu->regs.main.hl, 6); break;
    case 0xB7: alo_r(cpu, cpu->regs.main.a, 6); break;
    // cp
    case 0xB8: alo_r(cpu, cpu->regs.main.b, 7); break;
    case 0xB9: alo_r(cpu, cpu->regs.main.c, 7); break;
    case 0xBA: alo_r(cpu, cpu->regs.main.d, 7); break;
    case 0xBB: alo_r(cpu, cpu->regs.main.e, 7); break;
    case 0xBC: alo_r(cpu, cpu->regs.main.h, 7); break;
    case 0xBD: alo_r(cpu, cpu->regs.main.l, 7); break;
    case 0xBE: alo_rra(cpu, cpu->regs.main.hl, 7); break;
    case 0xBF: alo_r(cpu, cpu->regs.main.a, 7); break;

    // add/adc/sub/sbc/and/xor/or/cp n
    case 0xC6: alo_n(cpu, 0); break;
    case 0xCE: alo_n(cpu, 1); break;
    case 0xD6: alo_n(cpu, 2); break;
    case 0xDE: alo_n(cpu, 3); break;
    case 0xE6: alo_n(cpu, 4); break;
    case 0xEE: alo_n(cpu, 5); break;
    case 0xF6: alo_n(cpu, 6); break;
    case 0xFE: alo_n(cpu, 7); break;

    // djnz
    case 0x10: djnz_d(cpu); break;
    // jr
    case 0x18: jr_cc_d(cpu, true); break;
    // jr cc, d
    case 0x20: jr_cc_d(cpu, !cpu->regs.main.flags.z); break;
    case 0x28: jr_cc_d(cpu, cpu->regs.main.flags.z); break;
    case 0x30: jr_cc_d(cpu, !cpu->regs.main.flags.c); break;
    case 0x38: jr_cc_d(cpu, cpu->regs.main.flags.c); break;
    // jp cc, nn
    case 0xC2: jp_cc_nn(cpu, !cpu->regs.main.flags.z); break;
    case 0xCA: jp_cc_nn(cpu, cpu->regs.main.flags.z); break;
    case 0xD2: jp_cc_nn(cpu, !cpu->regs.main.flags.c); break;
    case 0xDA: jp_cc_nn(cpu, cpu->regs.main.flags.c); break;
    case 0xE2: jp_cc_nn(cpu, !cpu->regs.main.flags.pv); break;
    case 0xEA: jp_cc_nn(cpu, cpu->regs.main.flags.pv); break;
    case 0xF2: jp_cc_nn(cpu, !cpu->regs.main.flags.s); break;
    case 0xFA: jp_cc_nn(cpu, cpu->regs.main.flags.s); break;
    // jp nn
    case 0xC3: jp_cc_nn(cpu, true); break;
    // jp hl
    case 0xE9: jp_rr(cpu, cpu->regs.main.hl); break;

    // call cc, nn
    case 0xC4: call_cc_nn(cpu, !cpu->regs.main.flags.z); break;
    case 0xCC: call_cc_nn(cpu, cpu->regs.main.flags.z); break;
    case 0xD4: call_cc_nn(cpu, !cpu->regs.main.flags.c); break;
    case 0xDC: call_cc_nn(cpu, cpu->regs.main.flags.c); break;
    case 0xE4: call_cc_nn(cpu, !cpu->regs.main.flags.pv); break;
    case 0xEC: call_cc_nn(cpu, cpu->regs.main.flags.pv); break;
    case 0xF4: call_cc_nn(cpu, !cpu->regs.main.flags.s); break;
    case 0xFC: call_cc_nn(cpu, cpu->regs.main.flags.s); break;
    // call nn
    case 0xCD: call_cc_nn(cpu, true); break;
    // ret cc
    case 0xC0: ret_cc(cpu, !cpu->regs.main.flags.z); break;
    case 0xC8: ret_cc(cpu, cpu->regs.main.flags.z); break;
    case 0xD0: ret_cc(cpu, !cpu->regs.main.flags.c); break;
    case 0xD8: ret_cc(cpu, cpu->regs.main.flags.c); break;
    case 0xE0: ret_cc(cpu, !cpu->regs.main.flags.pv); break;
    case 0xE8: ret_cc(cpu, cpu->regs.main.flags.pv); break;
    case 0xF0: ret_cc(cpu, !cpu->regs.main.flags.s); break;
    case 0xF8: ret_cc(cpu, cpu->regs.main.flags.s); break;
    // ret (same behavior as imaginary instruction pop pc)
    case 0xC9: ret(cpu); break;

    // rst
    case 0xC7: rst(cpu, 0x00); break;
    case 0xCF: rst(cpu, 0x08); break;
    case 0xD7: rst(cpu, 0x10); break;
    case 0xDF: rst(cpu, 0x18); break;
    case 0xE7: rst(cpu, 0x20); break;
    case 0xEF: rst(cpu, 0x28); break;
    case 0xF7: rst(cpu, 0x30); break;
    case 0xFF: rst(cpu, 0x38); break;

    // pop
    case 0xC1: pop(cpu, &cpu->regs.main.bc); break;
    case 0xD1: pop(cpu, &cpu->regs.main.de); break;
    case 0xE1: pop(cpu, &cpu->regs.main.hl); break;
    case 0xF1: pop(cpu, &cpu->regs.main.af); break;
    // push
    case 0xC5: push(cpu, cpu->regs.main.bc); break;
    case 0xD5: push(cpu, cpu->regs.main.de); break;
    case 0xE5: push(cpu, cpu->regs.main.hl); break;
    case 0xF5: push(cpu, cpu->regs.main.af); break;

    // out (n), a
    case 0xD3: out_na_a(cpu); break;
    // in a, (n)
    case 0xDB: in_a_na(cpu); break;

    // ex af
    case 0x08: ex_af(cpu); break;
    // exx
    case 0xD9: exx(cpu); break;
    // XD
    case 0xEB: ex_de_hl(cpu); break;
    // ex (sp), hl
    case 0xE3: ex_spa_rr(cpu, &cpu->regs.main.hl); break;

    // DI / EI
    case 0xF3: di(cpu); break;
    case 0xFB: ei(cpu); break;
    // halt
    case 0x76: halt(cpu); break;

    case 0x00: nop(cpu); break;

    // bit instructions
    case 0xCB: do_cb(cpu); break;
    // misc instructions
    case 0xED: do_ed(cpu); break;

    // IY/IX prefix
    case 0xDD: ddfd(cpu, false); break;
    case 0xFD: ddfd(cpu, true); break;

    default:
        print_regs(cpu);
        dlog(LOG_ERR, "unimplemented opcode %02X at %04X", op, cpu->regs.pc);
        cpu->error = 1;
    }
}

void cpu_fire_interrupt(Z80_t *cpu)
{
    if (cpu->regs.iff1) {
        cpu->interrupt_pending = 1;
    }
}

static inline bool cpu_can_process_interrupts(Z80_t *cpu)
{
    if (cpu->prefix_state == STATE_NOPREFIX && !cpu->last_ei)
        return true;
    else
        return false;
}

int cpu_do_cycles(Z80_t *cpu)
{
    uint64_t cyc_old = cpu->cycles;

    cpu->regs.q_old = cpu->regs.q;
    cpu->regs.q = false;

    if (cpu->interrupt_pending && cpu_can_process_interrupts(cpu)) {
        inc_refresh(cpu);
        if (cpu->halted) {
            cpu->halted = false;
            cpu->regs.pc++;
        }
        switch (cpu->regs.im)
        {
        case 0: // IM 0 is irrel on speccy beyond 0xFF (rst 38) under typical circumstances
            cpu->regs.iff1 = 0;
            cpu->regs.iff2 = 0;
            cpu->cycles += 6;
            cpu->regs.sp--;
            cpu_write(cpu, cpu->regs.sp, HIGH8(cpu->regs.pc));
            cpu->cycles += 3;
            cpu->regs.sp--;
            cpu_write(cpu, cpu->regs.sp, LOW8(cpu->regs.pc));
            cpu->cycles += 3;
            cpu->regs.pc = 0x38;
            cpu->regs.memptr = cpu->regs.pc;
            break;
        case 1:
            cpu->regs.iff1 = 0;
            cpu->regs.iff2 = 0;
            cpu->cycles += 7;
            cpu->regs.sp--;
            cpu_write(cpu, cpu->regs.sp, HIGH8(cpu->regs.pc));
            cpu->cycles += 3;
            cpu->regs.sp--;
            cpu_write(cpu, cpu->regs.sp, LOW8(cpu->regs.pc));
            cpu->cycles += 3;
            cpu->regs.pc = 0x38;
            cpu->regs.memptr = cpu->regs.pc;
            break;
        case 2:
            cpu->regs.iff1 = 0;
            cpu->regs.iff2 = 0;
            cpu->regs.sp--;
            cpu->cycles += 7;
            cpu_write(cpu, cpu->regs.sp, HIGH8(cpu->regs.pc));
            cpu->regs.sp--;
            cpu->cycles += 3;
            cpu_write(cpu, cpu->regs.sp, LOW8(cpu->regs.pc));
            cpu->cycles += 3;
            uint16_t addr = MAKE16(0xFF, cpu->regs.i);
            uint8_t l = cpu_read(cpu, addr);
            cpu->cycles += 3;
            uint8_t h = cpu_read(cpu, addr+1);
            addr = MAKE16(l, h);
            cpu->regs.pc = addr;
            cpu->regs.memptr = cpu->regs.pc;
            cpu->cycles += 3;
            break;
        }
    } else if (cpu->halted) {
        inc_refresh(cpu);
        cpu_read(cpu, cpu->regs.pc+1);
        cpu->cycles += 4;
    } else {
        cpu->last_ei = false;
        switch (cpu->prefix_state) 
        {
        case STATE_DD: do_ddfd(cpu, false); break;
        case STATE_FD: do_ddfd(cpu, true); break;
        default: do_opcode(cpu); break;
        }
    }

    cpu->interrupt_pending = false;

    return cpu->cycles - cyc_old;
}