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

void print_regs(Z80_t *cpu)
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

/* SUB helper */
static inline uint8_t sub8(Z80_t *cpu, uint8_t a, uint8_t value)
{
    uint8_t result = a - value;
    cpu->regs.main.f &= ~MASK_FLAG_XY;
    cpu->regs.main.f |= (result & MASK_FLAG_XY);
    cpu->regs.main.flags.s = result & (1<<7);
    cpu->regs.main.flags.z = !result;
    cpu->regs.main.flags.pv = flag_overflow_8(a, value, 0, true);
    cpu->regs.main.flags.h = (a ^ result ^ value) & 0x10;
    cpu->regs.main.flags.c = (a < value);
    cpu->regs.main.flags.n = 1;
    return result;
}

static inline uint8_t dec8(Z80_t *cpu, uint8_t value);

/* instruction implementations */

/* 8-Bit Load Group */

/* LD r, r */
void ld_r_r(Z80_t *cpu, uint8_t *dest, uint8_t value)
{
    *dest = value;
    cpu->cycles += 4;
    cpu->regs.pc += 1;
}

/* LD r, (rr) */
void ld_r_rra(Z80_t *cpu, uint8_t *dest, uint16_t addr)
{
    // FIXME: placeholder, possibly wrong timings
    cpu->cycles += 4;
    *dest = cpu_read(cpu, addr);
    cpu->cycles += 3;
    cpu->regs.pc += 1;
}

/* LD r, n */
void ld_r_n(Z80_t *cpu, uint8_t *dest)
{
    // FIXME: placeholder, possibly wrong timings
    cpu->cycles += 4;
    cpu->regs.pc++;
    *dest = cpu_read(cpu, cpu->regs.pc);
    cpu->cycles += 3;
    cpu->regs.pc++;
}

/* LD r, (nn) */
void ld_r_nna(Z80_t *cpu, uint8_t *dest)
{
    // FIXME: placeholder, possibly wrong timings
    cpu->cycles += 4;
    cpu->regs.pc++;
    uint8_t l = cpu_read(cpu, cpu->regs.pc);
    cpu->cycles += 3;
    cpu->regs.pc++;
    uint8_t h = cpu_read(cpu, cpu->regs.pc);
    cpu->cycles += 3;
    cpu->regs.pc++;
    *dest = cpu_read(cpu, MAKE16(l, h));
    cpu->cycles += 3;
}

/* LD (rr), n */
void ld_rra_n(Z80_t *cpu, uint16_t addr)
{
    // FIXME: placeholder, possibly wrong timings
    cpu->cycles += 4;
    cpu->regs.pc++;
    uint8_t value = cpu_read(cpu, cpu->regs.pc);
    cpu->cycles += 3;
    cpu->regs.pc++;
    cpu_write(cpu, addr, value);
    cpu->cycles += 3;
}

/* ld i, a */
void ld_i_a(Z80_t *cpu, uint8_t *dest)
{
    *dest = cpu->regs.main.a;
    cpu->regs.pc++;
    cpu->cycles += 5;
}

/* ld a, i */
void ld_a_i(Z80_t *cpu, uint8_t value)
{
    cpu->regs.pc++;
    cpu->cycles += 5;

    cpu->regs.main.f &= ~MASK_FLAG_XY;
    cpu->regs.main.f |= (value & MASK_FLAG_XY);
    cpu->regs.main.flags.s = value & (1<<7);
    cpu->regs.main.flags.z = !value;
    cpu->regs.main.flags.h = 0;
    cpu->regs.main.flags.pv = cpu->regs.iff2;
    cpu->regs.main.flags.n = 0;

    cpu->regs.main.a = value;
}

/* LD (nn), a */
void ld_nna_a(Z80_t *cpu)
{
    // FIXME: placeholder, possibly wrong timings
    cpu->cycles += 4;
    cpu->regs.pc++;
    uint8_t l = cpu_read(cpu, cpu->regs.pc);
    cpu->cycles += 3;
    cpu->regs.pc++;
    uint8_t h = cpu_read(cpu, cpu->regs.pc);
    int16_t addr = MAKE16(l, h);
    cpu->cycles += 3;
    cpu->regs.pc++;
    cpu_write(cpu, addr, cpu->regs.main.a);
    cpu->cycles += 3;
}

/* LD (rr), r */
void ld_rra_r(Z80_t *cpu, uint16_t addr, uint8_t value)
{
    cpu->cycles += 4;
    cpu->regs.pc++;
    cpu_write(cpu, addr, value);
    cpu->cycles += 3;
}

/* LD (ii+d), r */
void ld_iid_r(Z80_t *cpu, uint16_t addr, uint8_t value)
{
    cpu->cycles += 4;
    cpu->regs.pc++;
    int8_t d = (int8_t)cpu_read(cpu, cpu->regs.pc);
    addr += d;
    cpu->cycles += 8; // amstrad gate 
    cpu->regs.pc++;
    cpu_write(cpu, addr, value);
    cpu->cycles += 3;
}

/* LD r, (ii+d) */
void ld_r_iid(Z80_t *cpu, uint8_t *dest, uint16_t addr)
{
    cpu->cycles += 4;
    cpu->regs.pc++;
    int8_t d = (int8_t)cpu_read(cpu, cpu->regs.pc);
    addr += d;
    cpu->cycles += 8; // amstrad gate 
    cpu->regs.pc++;
    *dest = cpu_read(cpu, addr);
    cpu->cycles += 3;
}

/* LD (ii+d), n */
void ld_iid_n(Z80_t *cpu, uint16_t addr)
{
    cpu->cycles += 4;
    cpu->regs.pc++;
    int8_t d = (int8_t)cpu_read(cpu, cpu->regs.pc);
    addr += d;
    cpu->cycles += 3; // amstrad gate 
    cpu->regs.pc++;
    uint8_t value = cpu_read(cpu, cpu->regs.pc);
    cpu->cycles += 5;
    cpu->regs.pc++;
    cpu_write(cpu, addr, value);
    cpu->cycles += 3;
}


/* 16-Bit Load Group */

/* LD rr, nn */
void ld_rr_nn(Z80_t *cpu, uint16_t *dest)
{
    // FIXME: placeholder, possibly wrong timings
    cpu->cycles += 4;
    cpu->regs.pc++;
    uint8_t l = cpu_read(cpu, cpu->regs.pc);
    cpu->cycles += 3;
    cpu->regs.pc++;
    uint8_t h = cpu_read(cpu, cpu->regs.pc);
    *dest = MAKE16(l, h); // FIXME: figure out a nicer macro for this 
    cpu->cycles += 3;
    cpu->regs.pc++;
}

/* LD rr, (nn) */
void ld_rr_nna(Z80_t *cpu, uint16_t *dest)
{
    // FIXME: placeholder, possibly wrong timings
    cpu->cycles += 4;
    cpu->regs.pc++;
    uint8_t l = cpu_read(cpu, cpu->regs.pc);
    cpu->cycles += 3;
    cpu->regs.pc++;
    uint8_t h = cpu_read(cpu, cpu->regs.pc);
    uint16_t addr = MAKE16(l, h); // FIXME: figure out a nicer macro for this 
    cpu->cycles += 3;
    cpu->regs.pc++;
    l = cpu_read(cpu, addr);
    cpu->cycles += 3;
    h = cpu_read(cpu, addr+1);
    cpu->cycles += 3;
    *dest = MAKE16(l, h);
}

/* LD (nn), rr */
void ld_nna_rr(Z80_t *cpu, uint16_t value)
{
    // FIXME: placeholder, possibly wrong timings
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
    cpu_write(cpu, addr+1, HIGH8(value));
    cpu->cycles += 3;
}

void ld_sp_rr(Z80_t *cpu, uint16_t value)
{
    cpu->cycles += 6;
    cpu->regs.pc++;
    cpu->regs.sp = value;
}

void pop(Z80_t *cpu, uint16_t *dest)
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

void ret(Z80_t *cpu)
{
    pop(cpu, &cpu->regs.pc);
}

void retn(Z80_t *cpu)
{
    pop(cpu, &cpu->regs.pc);
    cpu->regs.iff1 = cpu->regs.iff2;
}

void reti(Z80_t *cpu)
{
    // don't care about correct behavior since it's irrelevant on speccy
    pop(cpu, &cpu->regs.pc);
}

void push(Z80_t *cpu, uint16_t value)
{
    cpu->cycles += 5;
    cpu->regs.pc++;
    cpu->regs.sp--;
    cpu_write(cpu, cpu->regs.sp, HIGH8(value));
    cpu->cycles += 3;
    cpu->regs.sp--;
    cpu_write(cpu, cpu->regs.sp, LOW8(value));
    cpu->cycles += 3;
}

/* Exchange, Block Transfer and Search Group */

void exx(Z80_t *cpu)
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

void ex_de_hl(Z80_t *cpu)
{
    uint16_t tmp;
    tmp = cpu->regs.main.hl;
    cpu->regs.main.hl = cpu->regs.main.de;
    cpu->regs.main.de = tmp;

    cpu->cycles += 4;
    cpu->regs.pc++;
}

void ex_af(Z80_t *cpu)
{
    uint16_t tmp;
    tmp = cpu->regs.main.af;
    cpu->regs.main.af = cpu->regs.alt.af;
    cpu->regs.alt.af = tmp;

    cpu->cycles += 4;
    cpu->regs.pc++;
}

void ex_spa_rr(Z80_t *cpu, uint16_t *dest)
{
    cpu->cycles += 4;
    cpu->regs.pc++;
    uint8_t l = cpu_read(cpu, cpu->regs.sp);
    cpu->cycles += 3;
    uint8_t h = cpu_read(cpu, cpu->regs.sp+1);
    cpu->cycles += 4;
    cpu_write(cpu, cpu->regs.sp+1, HIGH8(*dest));
    cpu->cycles += 3;
    cpu_write(cpu, cpu->regs.sp, LOW8(*dest));
    cpu->cycles += 5;
    *dest = MAKE16(l, h);
}

/* LDI/LDD */
void ldx(Z80_t *cpu, int8_t increment)
{
    cpu->cycles += 4;
    cpu->regs.pc++;
    uint8_t value = cpu_read(cpu, cpu->regs.main.hl);
    cpu->cycles += 3;
    cpu_write(cpu, cpu->regs.main.de, value);
    // FIXME: unaccurate contention timings 
    cpu->cycles += 5;
    cpu->regs.main.hl += increment;
    cpu->regs.main.de += increment;
    cpu->regs.main.bc--;

    cpu->regs.main.flags.pv = !(!cpu->regs.main.bc);
    cpu->regs.main.flags.h = 0;
    cpu->regs.main.flags.n = 0;
}

/* LDIR/LDDR */
void ldxr(Z80_t *cpu, int8_t increment)
{
    ldx(cpu, increment);

    if (cpu->regs.main.bc != 0) {
        cpu->regs.pc -= 2;
        cpu->cycles += 5;
    }
}

/* CPI/CPD */
void cpx(Z80_t *cpu, int8_t increment)
{
    cpu->cycles += 4;
    cpu->regs.pc++;
    uint8_t value = cpu_read(cpu, cpu->regs.main.hl);
    cpu->cycles += 8;
    cpu->regs.main.bc--;
    cpu->regs.main.hl += increment;

    bool c = cpu->regs.main.flags.c;
    sub8(cpu, cpu->regs.main.a, value);
    uint8_t n = cpu->regs.main.a - value - cpu->regs.main.h;
    cpu->regs.main.flags.y = n & (1<<1);
    cpu->regs.main.flags.x = n & (1<<3); 
    cpu->regs.main.flags.pv = !(!cpu->regs.main.bc);
    cpu->regs.main.flags.c = c;
}

/* CPIR/CPDR */
void cpxr(Z80_t *cpu, int8_t increment)
{
    cpx(cpu, increment);

    if (cpu->regs.main.bc != 0 && !cpu->regs.main.flags.z) {
        cpu->regs.pc -= 2;
        cpu->cycles += 5;
    }
}

/* OUTI/OUTD */
void outx(Z80_t *cpu, int8_t increment)
{
    cpu->cycles += 4;
    cpu->regs.pc++;
    uint8_t value = cpu_read(cpu, cpu->regs.main.hl);
    cpu->cycles += 3;
    cpu_out(cpu, cpu->regs.main.bc, value);
    // FIXME: inaccurate timings?
    cpu->cycles += 5;
    cpu->regs.main.hl += increment;

    cpu->regs.main.b = dec8(cpu, cpu->regs.main.b);
    uint16_t k = cpu->regs.main.l + value;
    cpu->regs.main.flags.h = k > 255;
    cpu->regs.main.flags.c = k > 255;
    cpu->regs.main.flags.pv = get_parity((k & 7) ^ cpu->regs.main.b);
    cpu->regs.main.flags.n = value & (1<<7);
}

/* OTIR/OTDR */
void otxr(Z80_t *cpu, int8_t increment)
{
    outx(cpu, increment);

    if (cpu->regs.main.b != 0) {
        cpu->regs.pc -= 2;
        cpu->cycles += 5;
    }
}

/* INI/IND */
void inx(Z80_t *cpu, int8_t increment)
{
    cpu->cycles += 5;
    cpu->regs.pc++;
    cpu->regs.main.b = dec8(cpu, cpu->regs.main.b);

    uint8_t value = cpu_in(cpu, cpu->regs.main.bc);
    cpu->cycles += 4;
    cpu_write(cpu, cpu->regs.main.hl, value);
    // FIXME: inaccurate timings?
    cpu->cycles += 3;
    cpu->regs.main.hl += increment;

    uint16_t k = ((cpu->regs.main.c + increment) & 255) + value;
    cpu->regs.main.flags.h = k > 255;
    cpu->regs.main.flags.c = k > 255;
    cpu->regs.main.flags.pv = get_parity((k & 7) ^ cpu->regs.main.b);
    cpu->regs.main.flags.n = value & (1<<7);

    if (cpu->regs.main.b != 0) {
        cpu->regs.pc -= 2;
        cpu->cycles += 5;
    }
}

/* INIR/INDR */
void inxr(Z80_t *cpu, int8_t increment)
{
    inx(cpu, increment);

    if (cpu->regs.main.b != 0) {
        cpu->regs.pc -= 2;
        cpu->cycles += 5;
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
    cpu->regs.main.flags.h = !(value & 0x0F); // FIXME: not sure about correctness of this
    cpu->regs.main.flags.pv = value == 0x80;
    cpu->regs.main.flags.n = 0;
    return value;
}

/* INC r */
void inc_r(Z80_t *cpu, uint8_t *dest)
{
    cpu->cycles += 4;
    cpu->regs.pc++;
    *dest = inc8(cpu, *dest);
}

/* INC (rr) */
void inc_rra(Z80_t *cpu, uint16_t addr)
{
    cpu->cycles += 4;
    cpu->regs.pc++;
    uint8_t value = cpu_read(cpu, addr);
    cpu->cycles += 4;
    value = inc8(cpu, value);
    cpu_write(cpu, addr, value);
    cpu->cycles += 3;
}

/* INC (ii+d) */
void inc_iid(Z80_t *cpu, uint16_t addr)
{
    cpu->cycles += 4;
    cpu->regs.pc++;
    int8_t d = (int8_t)cpu_read(cpu, cpu->regs.pc);
    cpu->cycles += 8; // FIXME
    cpu->regs.pc++;
    addr += d;
    uint8_t value = cpu_read(cpu, addr);
    cpu->cycles += 4;
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
    cpu->regs.main.flags.h = (value & 0x0F) == 0x0F; // FIXME: not sure about correctness of this
    cpu->regs.main.flags.pv = value == 0x7F;
    cpu->regs.main.flags.n = 1;
    return value;
}

/* DEC r */
void dec_r(Z80_t *cpu, uint8_t *dest)
{
    cpu->cycles += 4;
    cpu->regs.pc++;
    *dest = dec8(cpu, *dest);
}

/* DEC (rr) */
void dec_rra(Z80_t *cpu, uint16_t addr)
{
    cpu->cycles += 4;
    cpu->regs.pc++;
    uint8_t value = cpu_read(cpu, addr);
    cpu->cycles += 4;
    value = dec8(cpu, value);
    cpu_write(cpu, addr, value);
    cpu->cycles += 3;
}

/* DEC (ii+d) */
void dec_iid(Z80_t *cpu, uint16_t addr)
{
    cpu->cycles += 4;
    cpu->regs.pc++;
    int8_t d = (int8_t)cpu_read(cpu, cpu->regs.pc);
    cpu->cycles += 8; // FIXME
    cpu->regs.pc++;
    addr += d;
    uint8_t value = cpu_read(cpu, addr);
    cpu->cycles += 4;
    value = dec8(cpu, value);
    cpu_write(cpu, addr, value);
    cpu->cycles += 3;
}

/* ADD helper */
static inline uint8_t add8(Z80_t *cpu, uint8_t value)
{
    uint8_t a = cpu->regs.main.a;
    uint8_t result = a + value;
    cpu->regs.main.f &= ~MASK_FLAG_XY;
    cpu->regs.main.f |= (value & MASK_FLAG_XY);
    cpu->regs.main.flags.s = result & (1<<7);
    cpu->regs.main.flags.z = !result;
    cpu->regs.main.flags.pv = flag_overflow_8(a, value, 0, false);
    cpu->regs.main.flags.h = (a ^ result ^ value) & 0x10;
    cpu->regs.main.flags.c = ((uint16_t)a + (uint16_t)value) > 255;
    cpu->regs.main.flags.n = 0;
    return result;
}

/* ADD A, r */
void add_a_r(Z80_t *cpu, uint8_t value)
{
    cpu->cycles += 4;
    cpu->regs.pc++;
    cpu->regs.main.a = add8(cpu, value);
}

/* ADD A, n */
void add_a_n(Z80_t *cpu)
{
    cpu->cycles += 4;
    cpu->regs.pc++;
    uint8_t value = cpu_read(cpu, cpu->regs.pc);
    cpu->cycles += 3;
    cpu->regs.pc++;
    cpu->regs.main.a = add8(cpu, value);
}

/* ADD A, (rr) */
void add_a_rra(Z80_t *cpu, uint16_t addr)
{
    cpu->cycles += 4;
    cpu->regs.pc++;
    uint8_t value = cpu_read(cpu, addr);
    cpu->cycles += 3;
    cpu->regs.main.a = add8(cpu, value);
}

/* ADD a, (ii+d) */
void add_a_iid(Z80_t *cpu, uint16_t addr)
{
    cpu->cycles += 4;
    cpu->regs.pc++;
    int8_t d = (int8_t)cpu_read(cpu, cpu->regs.pc);
    addr += d;
    cpu->cycles += 8; // FIXME
    cpu->regs.pc++;
    uint8_t value = cpu_read(cpu, addr);
    cpu->cycles += 3;
    cpu->regs.main.a = add8(cpu, value);
}

/* ADC helper */
static inline uint8_t adc8(Z80_t *cpu, uint8_t value)
{
    uint8_t a = cpu->regs.main.a;
    uint8_t result = a + value + cpu->regs.main.flags.c;
    cpu->regs.main.f &= ~MASK_FLAG_XY;
    cpu->regs.main.f |= (result & MASK_FLAG_XY);
    cpu->regs.main.flags.s = result & (1<<7);
    cpu->regs.main.flags.z = !result;
    cpu->regs.main.flags.pv = flag_overflow_8(a, value, cpu->regs.main.flags.c, false);
    cpu->regs.main.flags.h = (a ^ result ^ value) & 0x10;
    cpu->regs.main.flags.c = ((uint16_t)a + (uint16_t)value + 
                              cpu->regs.main.flags.c) > 255;
    cpu->regs.main.flags.n = 0;
    return result;
}

/* ADC A, r */
void adc_a_r(Z80_t *cpu, uint8_t value)
{
    cpu->cycles += 4;
    cpu->regs.pc++;
    cpu->regs.main.a = adc8(cpu, value);
}

/* ADC A, n */
void adc_a_n(Z80_t *cpu)
{
    cpu->cycles += 4;
    cpu->regs.pc++;
    uint8_t value = cpu_read(cpu, cpu->regs.pc);
    cpu->cycles += 3;
    cpu->regs.pc++;
    cpu->regs.main.a = adc8(cpu, value);
}

/* ADC A, (rr) */
void adc_a_rra(Z80_t *cpu, uint16_t addr)
{
    cpu->cycles += 4;
    cpu->regs.pc++;
    uint8_t value = cpu_read(cpu, addr);
    cpu->cycles += 3;
    cpu->regs.main.a = adc8(cpu, value);
}

/* ADC a, (ii+d) */
void adc_a_iid(Z80_t *cpu, uint16_t addr)
{
    cpu->cycles += 4;
    cpu->regs.pc++;
    int8_t d = (int8_t)cpu_read(cpu, cpu->regs.pc);
    addr += d;
    cpu->cycles += 8; // FIXME
    cpu->regs.pc++;
    uint8_t value = cpu_read(cpu, addr);
    cpu->cycles += 3;
    cpu->regs.main.a = adc8(cpu, value);
}

/* SBC helper */
static inline uint8_t sbc8(Z80_t *cpu, uint8_t value)
{
    uint8_t a = cpu->regs.main.a;
    uint8_t result = a - value - cpu->regs.main.flags.c;
    cpu->regs.main.f &= ~MASK_FLAG_XY;
    cpu->regs.main.f |= (result & MASK_FLAG_XY);
    cpu->regs.main.flags.s = result & (1<<7);
    cpu->regs.main.flags.z = !result;
    cpu->regs.main.flags.pv = flag_overflow_8(a, value, cpu->regs.main.flags.c, true);
    cpu->regs.main.flags.h = (a ^ result ^ value) & 0x10;
    cpu->regs.main.flags.c = a < ((uint16_t)value + cpu->regs.main.flags.c);
    cpu->regs.main.flags.n = 1;
    return result;
}

/* SBC A, r */
void sbc_a_r(Z80_t *cpu, uint8_t value)
{
    cpu->cycles += 4;
    cpu->regs.pc++;
    cpu->regs.main.a = sbc8(cpu, value);
}

/* SBC A, n */
void sbc_a_n(Z80_t *cpu)
{
    cpu->cycles += 4;
    cpu->regs.pc++;
    uint8_t value = cpu_read(cpu, cpu->regs.pc);
    cpu->cycles += 3;
    cpu->regs.pc++;
    cpu->regs.main.a = sbc8(cpu, value);
}

/* SBC A, (rr) */
void sbc_a_rra(Z80_t *cpu, uint16_t addr)
{
    cpu->cycles += 4;
    cpu->regs.pc++;
    uint8_t value = cpu_read(cpu, addr);
    cpu->cycles += 3;
    cpu->regs.main.a = sbc8(cpu, value);
}

/* SBC a, (ii+d) */
void sbc_a_iid(Z80_t *cpu, uint16_t addr)
{
    cpu->cycles += 4;
    cpu->regs.pc++;
    int8_t d = (int8_t)cpu_read(cpu, cpu->regs.pc);
    addr += d;
    cpu->cycles += 8; // FIXME
    cpu->regs.pc++;
    uint8_t value = cpu_read(cpu, addr);
    cpu->cycles += 3;
    cpu->regs.main.a = sbc8(cpu, value);
}

/* SUB r */
void sub_r(Z80_t *cpu, uint8_t value)
{
    cpu->cycles += 4;
    cpu->regs.pc++;
    cpu->regs.main.a = sub8(cpu, cpu->regs.main.a, value);
}

/* SUB n */
void sub_n(Z80_t *cpu)
{
    cpu->cycles += 4;
    cpu->regs.pc++;
    uint8_t value = cpu_read(cpu, cpu->regs.pc);
    cpu->cycles += 3;
    cpu->regs.pc++;
    cpu->regs.main.a = sub8(cpu, cpu->regs.main.a, value);
}

/* SUB (rr) */
void sub_rra(Z80_t *cpu, uint16_t addr)
{
    cpu->cycles += 4;
    cpu->regs.pc++;
    uint8_t value = cpu_read(cpu, addr);
    cpu->cycles += 3;
    cpu->regs.main.a = sub8(cpu, cpu->regs.main.a, value);
}

/* SUB (ii+d) */
void sub_iid(Z80_t *cpu, uint16_t addr)
{
    cpu->cycles += 4;
    cpu->regs.pc++;
    int8_t d = (int8_t)cpu_read(cpu, cpu->regs.pc);
    addr += d;
    cpu->cycles += 8; // FIXME
    cpu->regs.pc++;
    uint8_t value = cpu_read(cpu, addr);
    cpu->cycles += 3;
    cpu->regs.main.a = sub8(cpu, cpu->regs.main.a, value);
}

/* CP r */
void cp_r(Z80_t *cpu, uint8_t value)
{
    cpu->cycles += 4;
    cpu->regs.pc++;
    sub8(cpu, cpu->regs.main.a, value);
}

/* CP n */
void cp_n(Z80_t *cpu)
{
    cpu->cycles += 4;
    cpu->regs.pc++;
    uint8_t value = cpu_read(cpu, cpu->regs.pc);
    cpu->cycles += 3;
    cpu->regs.pc++;
    sub8(cpu, cpu->regs.main.a, value);
}

/* CP (rr) */
void cp_rra(Z80_t *cpu, uint16_t addr)
{
    cpu->cycles += 4;
    cpu->regs.pc++;
    uint8_t value = cpu_read(cpu, addr);
    cpu->cycles += 3;
    sub8(cpu, cpu->regs.main.a, value);
}

/* CP (ii+d) */
void cp_iid(Z80_t *cpu, uint16_t addr)
{
    cpu->cycles += 4;
    cpu->regs.pc++;
    int8_t d = (int8_t)cpu_read(cpu, cpu->regs.pc);
    addr += d;
    cpu->cycles += 8; // FIXME
    cpu->regs.pc++;
    uint8_t value = cpu_read(cpu, addr);
    cpu->cycles += 3;
    sub8(cpu, cpu->regs.main.a, value);
}

/* AND helper */
static inline void and(Z80_t *cpu, uint8_t value)
{
    value &= cpu->regs.main.a;
    cpu->regs.main.a = value;
    cpu->regs.main.f &= ~MASK_FLAG_XY;
    cpu->regs.main.f |= (value & MASK_FLAG_XY);
    cpu->regs.main.flags.s = value & (1<<7);
    cpu->regs.main.flags.z = !value;
    cpu->regs.main.flags.h = 1;
    cpu->regs.main.flags.pv = get_parity(value);
    cpu->regs.main.flags.c = 0;
    cpu->regs.main.flags.n = 0;
}

/* AND r */
void and_r(Z80_t *cpu, uint8_t value)
{
    and(cpu, value);
    cpu->cycles += 4;
    cpu->regs.pc++;
}

/* AND (rr) */
void and_rra(Z80_t *cpu, uint16_t addr)
{
    // FIXME: placeholder, possibly wrong timings
    cpu->cycles += 4;
    uint8_t value = cpu_read(cpu, addr);
    and(cpu, value);
    cpu->cycles += 3;
    cpu->regs.pc++;
}

/* AND n */
void and_n(Z80_t *cpu)
{
    cpu->cycles += 4;
    cpu->regs.pc++;
    uint8_t value = cpu_read(cpu, cpu->regs.pc);
    cpu->cycles += 3;
    cpu->regs.pc++;
    and(cpu, value);
}

/* AND (ii+d) */
void and_iid(Z80_t *cpu, uint16_t addr)
{
    cpu->cycles += 4;
    cpu->regs.pc++;
    int8_t d = (int8_t)cpu_read(cpu, cpu->regs.pc);
    addr += d;
    cpu->cycles += 8; // FIXME
    cpu->regs.pc++;
    uint8_t value = cpu_read(cpu, addr);
    cpu->cycles += 3;
    and(cpu, value);
}

/* XOR helper */
static inline void xor(Z80_t *cpu, uint8_t value)
{
    value ^= cpu->regs.main.a;
    cpu->regs.main.a = value;
    cpu->regs.main.f &= ~MASK_FLAG_XY;
    cpu->regs.main.f |= (value & MASK_FLAG_XY);
    cpu->regs.main.flags.s = value & (1<<7);
    cpu->regs.main.flags.z = !value;
    cpu->regs.main.flags.h = 0;
    cpu->regs.main.flags.pv = get_parity(value);
    cpu->regs.main.flags.c = 0;
    cpu->regs.main.flags.n = 0;
}

/* XOR r */
void xor_r(Z80_t *cpu, uint8_t value)
{
    xor(cpu, value);
    cpu->cycles += 4;
    cpu->regs.pc++;
}

/* XOR (rr) */
void xor_rra(Z80_t *cpu, uint16_t addr)
{
    // FIXME: placeholder, possibly wrong timings
    cpu->cycles += 4;
    uint8_t value = cpu_read(cpu, addr);
    xor(cpu, value);
    cpu->cycles += 3;
    cpu->regs.pc++;
}

/* XOR n */
void xor_n(Z80_t *cpu)
{
    cpu->cycles += 4;
    cpu->regs.pc++;
    uint8_t value = cpu_read(cpu, cpu->regs.pc);
    cpu->cycles += 3;
    cpu->regs.pc++;
    xor(cpu, value);
}

/* XOR (ii+d) */
void xor_iid(Z80_t *cpu, uint16_t addr)
{
    cpu->cycles += 4;
    cpu->regs.pc++;
    int8_t d = (int8_t)cpu_read(cpu, cpu->regs.pc);
    addr += d;
    cpu->cycles += 8; // FIXME
    cpu->regs.pc++;
    uint8_t value = cpu_read(cpu, addr);
    cpu->cycles += 3;
    xor(cpu, value);
}

/* OR helper */
static inline void or(Z80_t *cpu, uint8_t value)
{
    value |= cpu->regs.main.a;
    cpu->regs.main.a = value;
    cpu->regs.main.f &= ~MASK_FLAG_XY;
    cpu->regs.main.f |= (value & MASK_FLAG_XY);
    cpu->regs.main.flags.s = value & (1<<7);
    cpu->regs.main.flags.z = !value;
    cpu->regs.main.flags.h = 0;
    cpu->regs.main.flags.pv = get_parity(value);
    cpu->regs.main.flags.c = 0;
    cpu->regs.main.flags.n = 0;
}

/* OR r */
void or_r(Z80_t *cpu, uint8_t value)
{
    or(cpu, value);
    cpu->cycles += 4;
    cpu->regs.pc++;
}

/* OR (rr) */
void or_rra(Z80_t *cpu, uint16_t addr)
{
    // FIXME: placeholder, possibly wrong timings
    cpu->cycles += 4;
    uint8_t value = cpu_read(cpu, addr);
    or(cpu, value);
    cpu->cycles += 3;
    cpu->regs.pc++;
}

/* OR n */
void or_n(Z80_t *cpu)
{
    cpu->cycles += 4;
    cpu->regs.pc++;
    uint8_t value = cpu_read(cpu, cpu->regs.pc);
    cpu->cycles += 3;
    cpu->regs.pc++;
    or(cpu, value);
}

/* OR (ii+d) */
void or_iid(Z80_t *cpu, uint16_t addr)
{
    cpu->cycles += 4;
    cpu->regs.pc++;
    int8_t d = (int8_t)cpu_read(cpu, cpu->regs.pc);
    addr += d;
    cpu->cycles += 8; // FIXME
    cpu->regs.pc++;
    uint8_t value = cpu_read(cpu, addr);
    cpu->cycles += 3;
    or(cpu, value);
}

/* General-Purpose Arithmetic and CPU Control Groups */

void daa(Z80_t *cpu)
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

    cpu->regs.main.a = result;
}

void cpl(Z80_t *cpu)
{
    cpu->cycles += 4;
    cpu->regs.pc++;
    cpu->regs.main.flags.h = 1;
    cpu->regs.main.flags.n = 1;
    cpu->regs.main.a ^= 0xFF;
}

void neg(Z80_t *cpu)
{
    cpu->cycles += 4;
    cpu->regs.pc++;
    uint8_t result = sub8(cpu, 0, cpu->regs.main.a);
    cpu->regs.main.flags.c = !(!cpu->regs.main.a);
    cpu->regs.main.flags.pv = (cpu->regs.main.a == 0x80);
    cpu->regs.main.a = result;
}

/* CCF/SCF */
void ccf(Z80_t *cpu)
{
    cpu->cycles += 4;
    cpu->regs.pc++;
    cpu->regs.main.flags.h = cpu->regs.main.flags.c;
    cpu->regs.main.flags.n = 0;
    cpu->regs.main.flags.c ^= 1;
}

void scf(Z80_t *cpu)
{
    cpu->cycles += 4;
    cpu->regs.pc++;
    cpu->regs.main.flags.h = 0;
    cpu->regs.main.flags.n = 0;
    cpu->regs.main.flags.c = 1;
}

/* IM 0/1/2 */
void im(Z80_t *cpu, uint8_t im)
{
    cpu->cycles += 4;
    cpu->regs.pc++;
    cpu->regs.im = im;
}


/* 16-Bit Arithmetic Group */

void inc_rr(Z80_t *cpu, uint16_t *dest)
{
    cpu->cycles += 6;
    cpu->regs.pc++;
    *dest += 1;
}

void dec_rr(Z80_t *cpu, uint16_t *dest)
{
    cpu->cycles += 6;
    cpu->regs.pc++;
    *dest -= 1;
}

void add_rr_rr(Z80_t *cpu, uint16_t *dest, uint16_t value)
{
    uint32_t result = (uint32_t)*dest + value;
    cpu->regs.main.flags.h = (*dest ^ result ^ value) & 0x100;
    cpu->regs.main.flags.n = 0;
    cpu->regs.main.flags.c = result & (1<<16); // hmm thats kinda stupid
    *dest = (uint16_t)result;
    cpu->cycles += 11;
    cpu->regs.pc++;
}

void adc_rr_rr(Z80_t *cpu, uint16_t *dest, uint16_t value)
{
    uint32_t result = (uint32_t)*dest + value + cpu->regs.main.flags.c;
    cpu->regs.main.flags.s = result & (1<<15);
    cpu->regs.main.flags.z = !(result & 0xFFFF);
    cpu->regs.main.flags.h = (*dest ^ result ^ value) & 0x100;
    cpu->regs.main.flags.pv = flag_overflow_16(*dest, value, cpu->regs.main.flags.c, false);
    cpu->regs.main.flags.n = 0;
    cpu->regs.main.flags.c = result & (1<<16); // hmm thats kinda stupid
    *dest = (uint16_t)result;
    cpu->cycles += 11;
    cpu->regs.pc++;
}

void sbc_rr_rr(Z80_t *cpu, uint16_t *dest, uint16_t value)
{
    uint32_t result = (uint32_t)*dest - value - cpu->regs.main.flags.c;
    cpu->regs.main.flags.s = result & (1<<15);
    cpu->regs.main.flags.z = !(result & 0xFFFF);
    cpu->regs.main.flags.h = (*dest ^ result ^ value) & 0x100;
    cpu->regs.main.flags.pv = flag_overflow_16(*dest, value, cpu->regs.main.flags.c, true);
    cpu->regs.main.flags.n = 1;
    cpu->regs.main.flags.c = *dest < ((uint32_t)value + cpu->regs.main.flags.c);
    *dest = (uint16_t)result;
    cpu->cycles += 11;
    cpu->regs.pc++;
}

/* Rotate and Shift Group */

/* helpers */
static inline uint8_t rlc(Z80_t *cpu, uint8_t value)
{
    value = (value<<1) | (value>>7);
    cpu->regs.main.flags.c = value & 1;

    cpu->regs.main.f &= ~MASK_FLAG_XY;
    cpu->regs.main.f |= (value & MASK_FLAG_XY);
    cpu->regs.main.flags.s = value & (1<<7);
    cpu->regs.main.flags.z = !value;
    cpu->regs.main.flags.h = 0;
    cpu->regs.main.flags.pv = get_parity(value);
    cpu->regs.main.flags.n = 0;

    return value;
}

static inline uint8_t rrc(Z80_t *cpu, uint8_t value)
{
    cpu->regs.main.flags.c = value & 1;
    value = (value>>1) | (value<<7);

    cpu->regs.main.f &= ~MASK_FLAG_XY;
    cpu->regs.main.f |= (value & MASK_FLAG_XY);
    cpu->regs.main.flags.s = value & (1<<7);
    cpu->regs.main.flags.z = !value;
    cpu->regs.main.flags.h = 0;
    cpu->regs.main.flags.pv = get_parity(value);
    cpu->regs.main.flags.n = 0;

    return value;
}

static inline uint8_t rl(Z80_t *cpu, uint8_t value)
{
    bool c_new = (value>>7);
    value = (value<<1) | cpu->regs.main.flags.c;
    cpu->regs.main.flags.c = c_new;

    cpu->regs.main.f &= ~MASK_FLAG_XY;
    cpu->regs.main.f |= (value & MASK_FLAG_XY);
    cpu->regs.main.flags.s = value & (1<<7);
    cpu->regs.main.flags.z = !value;
    cpu->regs.main.flags.h = 0;
    cpu->regs.main.flags.pv = get_parity(value);
    cpu->regs.main.flags.n = 0;

    return value;
}

static inline uint8_t rr(Z80_t *cpu, uint8_t value)
{
    bool c_new = value & 1;
    value = (value>>1) | (cpu->regs.main.flags.c<<7);
    cpu->regs.main.flags.c = c_new;

    cpu->regs.main.f &= ~MASK_FLAG_XY;
    cpu->regs.main.f |= (value & MASK_FLAG_XY);
    cpu->regs.main.flags.s = value & (1<<7);
    cpu->regs.main.flags.z = !value;
    cpu->regs.main.flags.h = 0;
    cpu->regs.main.flags.pv = get_parity(value);
    cpu->regs.main.flags.n = 0;

    return value;
}

static inline uint8_t sla(Z80_t *cpu, uint8_t value)
{
    cpu->regs.main.flags.c = value>>7;
    value = (value<<1);

    cpu->regs.main.f &= ~MASK_FLAG_XY;
    cpu->regs.main.f |= (value & MASK_FLAG_XY);
    cpu->regs.main.flags.s = value & (1<<7);
    cpu->regs.main.flags.z = !value;
    cpu->regs.main.flags.h = 0;
    cpu->regs.main.flags.pv = get_parity(value);
    cpu->regs.main.flags.n = 0;

    return value;
}

static inline uint8_t sra(Z80_t *cpu, uint8_t value)
{
    cpu->regs.main.flags.c = value & 1;
    value = ((value & (1<<7)) | (value>>1));

    cpu->regs.main.f &= ~MASK_FLAG_XY;
    cpu->regs.main.f |= (value & MASK_FLAG_XY);
    cpu->regs.main.flags.s = value & (1<<7);
    cpu->regs.main.flags.z = !value;
    cpu->regs.main.flags.h = 0;
    cpu->regs.main.flags.pv = get_parity(value);
    cpu->regs.main.flags.n = 0;

    return value;
}

static inline uint8_t sll(Z80_t *cpu, uint8_t value)
{
    cpu->regs.main.flags.c = value>>7;
    value = (value<<1) | 1;

    cpu->regs.main.f &= ~MASK_FLAG_XY;
    cpu->regs.main.f |= (value & MASK_FLAG_XY);
    cpu->regs.main.flags.s = value & (1<<7);
    cpu->regs.main.flags.z = !value;
    cpu->regs.main.flags.h = 0;
    cpu->regs.main.flags.pv = get_parity(value);
    cpu->regs.main.flags.n = 0;

    return value;
}

static inline uint8_t srl(Z80_t *cpu, uint8_t value)
{
    cpu->regs.main.flags.c = value & 1;
    value = (value>>1);

    cpu->regs.main.f &= ~MASK_FLAG_XY;
    cpu->regs.main.f |= (value & MASK_FLAG_XY);
    cpu->regs.main.flags.s = value & (1<<7);
    cpu->regs.main.flags.z = !value;
    cpu->regs.main.flags.h = 0;
    cpu->regs.main.flags.pv = get_parity(value);
    cpu->regs.main.flags.n = 0;

    return value;
}

void rlca(Z80_t *cpu)
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
}

void rrca(Z80_t *cpu)
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
}

void rla(Z80_t *cpu)
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
}

void rra(Z80_t *cpu)
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
}



void rlc_r(Z80_t *cpu, uint8_t *dest)
{
    cpu->cycles += 4;
    cpu->regs.pc++;
    *dest = rlc(cpu, *dest);
}

void rlc_rra(Z80_t *cpu, uint16_t addr)
{
    cpu->cycles += 4;
    cpu->regs.pc++;
    uint8_t value = cpu_read(cpu, addr);
    cpu->cycles += 4;
    value = rlc(cpu, value);
    cpu_write(cpu, addr, value);
    cpu->cycles += 3;
}

void rlc_iid_r(Z80_t *cpu, uint16_t addr, uint8_t *dest)
{
    cpu->cycles += 4;
    cpu->regs.pc++;
    uint8_t value = cpu_read(cpu, addr);
    cpu->cycles += 4;
    value = rlc(cpu, value);
    cpu_write(cpu, addr, value);
    cpu->cycles += 3;
    if (dest) *dest = value;
}


void rrc_r(Z80_t *cpu, uint8_t *dest)
{
    cpu->cycles += 4;
    cpu->regs.pc++;
    *dest = rrc(cpu, *dest);
}

void rrc_rra(Z80_t *cpu, uint16_t addr)
{
    cpu->cycles += 4;
    cpu->regs.pc++;
    uint8_t value = cpu_read(cpu, addr);
    cpu->cycles += 4;
    value = rrc(cpu, value);
    cpu_write(cpu, addr, value);
    cpu->cycles += 3;
}

void rrc_iid_r(Z80_t *cpu, uint16_t addr, uint8_t *dest)
{
    cpu->cycles += 4;
    cpu->regs.pc++;
    uint8_t value = cpu_read(cpu, addr);
    cpu->cycles += 4;
    value = rrc(cpu, value);
    cpu_write(cpu, addr, value);
    cpu->cycles += 3;
    if (dest) *dest = value;
}


void rl_r(Z80_t *cpu, uint8_t *dest)
{
    cpu->cycles += 4;
    cpu->regs.pc++;
    *dest = rl(cpu, *dest);
}

void rl_rra(Z80_t *cpu, uint16_t addr)
{
    cpu->cycles += 4;
    cpu->regs.pc++;
    uint8_t value = cpu_read(cpu, addr);
    cpu->cycles += 4;
    value = rl(cpu, value);
    cpu_write(cpu, addr, value);
    cpu->cycles += 3;
}

void rl_iid_r(Z80_t *cpu, uint16_t addr, uint8_t *dest)
{
    cpu->cycles += 4;
    cpu->regs.pc++;
    uint8_t value = cpu_read(cpu, addr);
    cpu->cycles += 4;
    value = rl(cpu, value);
    cpu_write(cpu, addr, value);
    cpu->cycles += 3;
    if (dest) *dest = value;
}


void rr_r(Z80_t *cpu, uint8_t *dest)
{
    cpu->cycles += 4;
    cpu->regs.pc++;
    *dest = rr(cpu, *dest);
}

void rr_rra(Z80_t *cpu, uint16_t addr)
{
    cpu->cycles += 4;
    cpu->regs.pc++;
    uint8_t value = cpu_read(cpu, addr);
    cpu->cycles += 4;
    value = rr(cpu, value);
    cpu_write(cpu, addr, value);
    cpu->cycles += 3;
}

void rr_iid_r(Z80_t *cpu, uint16_t addr, uint8_t *dest)
{
    cpu->cycles += 4;
    cpu->regs.pc++;
    uint8_t value = cpu_read(cpu, addr);
    cpu->cycles += 4;
    value = rr(cpu, value);
    cpu_write(cpu, addr, value);
    cpu->cycles += 3;
    if (dest) *dest = value;
}


void sla_r(Z80_t *cpu, uint8_t *dest)
{
    cpu->cycles += 4;
    cpu->regs.pc++;
    *dest = sla(cpu, *dest);
}

void sla_rra(Z80_t *cpu, uint16_t addr)
{
    cpu->cycles += 4;
    cpu->regs.pc++;
    uint8_t value = cpu_read(cpu, addr);
    cpu->cycles += 4;
    value = sla(cpu, value);
    cpu_write(cpu, addr, value);
    cpu->cycles += 3;
}

void sla_iid_r(Z80_t *cpu, uint16_t addr, uint8_t *dest)
{
    cpu->cycles += 4;
    cpu->regs.pc++;
    uint8_t value = cpu_read(cpu, addr);
    cpu->cycles += 4;
    value = sla(cpu, value);
    cpu_write(cpu, addr, value);
    cpu->cycles += 3;
    if (dest) *dest = value;
}


void sra_r(Z80_t *cpu, uint8_t *dest)
{
    cpu->cycles += 4;
    cpu->regs.pc++;
    *dest = sra(cpu, *dest);
}

void sra_rra(Z80_t *cpu, uint16_t addr)
{
    cpu->cycles += 4;
    cpu->regs.pc++;
    uint8_t value = cpu_read(cpu, addr);
    cpu->cycles += 4;
    value = sra(cpu, value);
    cpu_write(cpu, addr, value);
    cpu->cycles += 3;
}

void sra_iid_r(Z80_t *cpu, uint16_t addr, uint8_t *dest)
{
    cpu->cycles += 4;
    cpu->regs.pc++;
    uint8_t value = cpu_read(cpu, addr);
    cpu->cycles += 4;
    value = sra(cpu, value);
    cpu_write(cpu, addr, value);
    cpu->cycles += 3;
    if (dest) *dest = value;
}


void sll_r(Z80_t *cpu, uint8_t *dest)
{
    cpu->cycles += 4;
    cpu->regs.pc++;
    *dest = sll(cpu, *dest);
}

void sll_rra(Z80_t *cpu, uint16_t addr)
{
    cpu->cycles += 4;
    cpu->regs.pc++;
    uint8_t value = cpu_read(cpu, addr);
    cpu->cycles += 4;
    value = sll(cpu, value);
    cpu_write(cpu, addr, value);
    cpu->cycles += 3;
}

void sll_iid_r(Z80_t *cpu, uint16_t addr, uint8_t *dest)
{
    cpu->cycles += 4;
    cpu->regs.pc++;
    uint8_t value = cpu_read(cpu, addr);
    cpu->cycles += 4;
    value = sll(cpu, value);
    cpu_write(cpu, addr, value);
    cpu->cycles += 3;
    if (dest) *dest = value;
}


void srl_r(Z80_t *cpu, uint8_t *dest)
{
    cpu->cycles += 4;
    cpu->regs.pc++;
    *dest = srl(cpu, *dest);
}

void srl_rra(Z80_t *cpu, uint16_t addr)
{
    cpu->cycles += 4;
    cpu->regs.pc++;
    uint8_t value = cpu_read(cpu, addr);
    cpu->cycles += 4;
    value = srl(cpu, value);
    cpu_write(cpu, addr, value);
    cpu->cycles += 3;
}

void srl_iid_r(Z80_t *cpu, uint16_t addr, uint8_t *dest)
{
    cpu->cycles += 4;
    cpu->regs.pc++;
    uint8_t value = cpu_read(cpu, addr);
    cpu->cycles += 4;
    value = srl(cpu, value);
    cpu_write(cpu, addr, value);
    cpu->cycles += 3;
    if (dest) *dest = value;
}

void rld(Z80_t *cpu)
{
    cpu->cycles += 4;
    cpu->regs.pc++;
    uint8_t value = cpu_read(cpu, cpu->regs.main.hl);
    cpu->cycles += 7; // amstrad gate

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

    cpu_write(cpu, cpu->regs.main.hl, value);
    cpu->cycles += 3;
}

void rrd(Z80_t *cpu)
{
    cpu->cycles += 4;
    cpu->regs.pc++;
    uint8_t value = cpu_read(cpu, cpu->regs.main.hl);
    cpu->cycles += 7; // amstrad gate

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

    cpu_write(cpu, cpu->regs.main.hl, value);
    cpu->cycles += 3;
}


/* Bit Set, Reset and Test Group */

static inline void bit_(Z80_t *cpu, uint8_t value, uint8_t bit)
{
    uint8_t mask = (1<<bit);
    value &= mask;
    cpu->regs.main.f &= ~MASK_FLAG_XY;
    cpu->regs.main.f |= (value & MASK_FLAG_XY);
    cpu->regs.main.flags.s = value & (1<<7);
    cpu->regs.main.flags.z = !value;
    cpu->regs.main.flags.h = 1;
    cpu->regs.main.flags.pv = get_parity(value);
    cpu->regs.main.flags.n = 0;
}

void bit_r(Z80_t *cpu, uint8_t value, uint8_t bit)
{
    cpu->cycles += 4;
    cpu->regs.pc++;
    bit_(cpu, value, bit);
}

void bit_rra(Z80_t *cpu, uint16_t addr, uint8_t bit)
{
    cpu->cycles += 4;
    cpu->regs.pc++;
    uint8_t value = cpu_read(cpu, addr);
    cpu->cycles += 4;
    bit_(cpu, value, bit);
}

static inline uint8_t res(Z80_t *cpu, uint8_t value, uint8_t bit)
{
    uint8_t mask = ~(1<<bit);
    return value & mask;
}

void res_r(Z80_t *cpu, uint8_t *dest, uint8_t bit)
{
    cpu->cycles += 4;
    cpu->regs.pc++;
    *dest = res(cpu, *dest, bit);
}

void res_rra(Z80_t *cpu, uint16_t addr, uint8_t bit)
{
    cpu->cycles += 4;
    cpu->regs.pc++;
    uint8_t value = cpu_read(cpu, addr);
    cpu->cycles += 4;
    value = res(cpu, value, bit);
    cpu_write(cpu, addr, value);
    cpu->cycles += 3;
}

void res_iid_r(Z80_t *cpu, uint16_t addr, uint8_t *dest, uint8_t bit)
{
    cpu->cycles += 4;
    cpu->regs.pc++;
    uint8_t value = cpu_read(cpu, addr);
    cpu->cycles += 4;
    value = res(cpu, value, bit);
    cpu_write(cpu, addr, value);
    cpu->cycles += 3;
    if (dest) *dest = value;
}


static inline uint8_t set(Z80_t *cpu, uint8_t value, uint8_t bit)
{
    uint8_t mask = (1<<bit);
    return value | mask;
}

void set_r(Z80_t *cpu, uint8_t *dest, uint8_t bit)
{
    cpu->cycles += 4;
    cpu->regs.pc++;
    *dest = set(cpu, *dest, bit);
}

void set_rra(Z80_t *cpu, uint16_t addr, uint8_t bit)
{
    cpu->cycles += 4;
    cpu->regs.pc++;
    uint8_t value = cpu_read(cpu, addr);
    cpu->cycles += 4;
    value = set(cpu, value, bit);
    cpu_write(cpu, addr, value);
    cpu->cycles += 3;
}

void set_iid_r(Z80_t *cpu, uint16_t addr, uint8_t *dest, uint8_t bit)
{
    cpu->cycles += 4;
    cpu->regs.pc++;
    uint8_t value = cpu_read(cpu, addr);
    cpu->cycles += 4;
    value = set(cpu, value, bit);
    cpu_write(cpu, addr, value);
    cpu->cycles += 3;
    if (dest) *dest = value;
}



/* Jump Group */

// FIXME: probably should remove this because its virtually the same as cc_nn :?
void jp_nn(Z80_t *cpu)
{
    // FIXME: placeholder, possibly wrong timings
    cpu->cycles += 4;
    cpu->regs.pc++;
    uint8_t l = cpu_read(cpu, cpu->regs.pc);
    cpu->cycles += 3;
    cpu->regs.pc++;
    uint8_t h = cpu_read(cpu, cpu->regs.pc);
    cpu->regs.pc = MAKE16(l, h); // FIXME: figure out a nicer macro for this 
    cpu->cycles += 3;
}

void jp_cc_nn(Z80_t *cpu, bool cc)
{
    // FIXME: placeholder, possibly wrong timings
    cpu->cycles += 4;
    cpu->regs.pc++;
    uint8_t l = cpu_read(cpu, cpu->regs.pc);
    cpu->cycles += 3;
    cpu->regs.pc++;
    uint8_t h = cpu_read(cpu, cpu->regs.pc);
    cpu->cycles += 3;
    cpu->regs.pc++;
    if (cc) {
        cpu->regs.pc = MAKE16(l, h); // FIXME: figure out a nicer macro for this 
    }
}

void jr_cc_d(Z80_t *cpu, bool cc)
{
    // FIXME: placeholder, possibly wrong timings
    cpu->cycles += 4;
    cpu->regs.pc++;
    int8_t offset = (int8_t)cpu_read(cpu, cpu->regs.pc);
    cpu->cycles += 3;
    cpu->regs.pc++;
    if (cc) {
        cpu->regs.pc += offset;
        cpu->cycles += 5;
    }
}

void jp_rr(Z80_t *cpu, uint16_t addr)
{
    cpu->cycles += 4;
    cpu->regs.pc = addr;
}

void djnz_d(Z80_t *cpu)
{
    // FIXME: placeholder, possibly wrong timings
    cpu->cycles += 5;
    cpu->regs.pc++;
    int8_t offset = (int8_t)cpu_read(cpu, cpu->regs.pc);
    cpu->cycles += 3;
    cpu->regs.pc++;
    cpu->regs.main.b--;
    if (cpu->regs.main.b) {
        cpu->regs.pc += offset;
        cpu->cycles += 5;
    }
}

/* Call and Return Group */

void call_cc_nn(Z80_t *cpu, bool cc)
{
    // FIXME: placeholder, possibly wrong timings
    cpu->cycles += 4;
    cpu->regs.pc++;
    uint8_t l = cpu_read(cpu, cpu->regs.pc);
    cpu->cycles += 3;
    cpu->regs.pc++;
    uint8_t h = cpu_read(cpu, cpu->regs.pc);
    cpu->cycles += 3;
    cpu->regs.pc++;
    if (cc) {
        cpu_read(cpu, cpu->regs.pc); // pc+2:1
        cpu->cycles++;
        cpu->regs.sp--;
        cpu_write(cpu, cpu->regs.sp, HIGH8(cpu->regs.pc));
        cpu->cycles += 3;
        cpu->regs.sp--;
        cpu_write(cpu, cpu->regs.sp, LOW8(cpu->regs.pc));
        cpu->cycles += 3;
        cpu->regs.pc = MAKE16(l, h); // FIXME: figure out a nicer macro for this
    }
}

// RET cc (and ONLY cc as timings are different)
void ret_cc(Z80_t *cpu, bool cc)
{
    cpu->cycles += 5;
    cpu->regs.pc++;
    if (cc) {
        uint8_t l = cpu_read(cpu, cpu->regs.sp);
        cpu->cycles += 3;
        cpu->regs.sp++;
        uint8_t h = cpu_read(cpu, cpu->regs.sp);
        cpu->cycles += 3;
        cpu->regs.sp++;
        cpu->regs.pc = MAKE16(l, h);
    }
}

void rst(Z80_t *cpu, uint8_t offset)
{
    cpu->cycles += 5;
    cpu->regs.pc++;
    cpu->regs.sp--;
    cpu_write(cpu, cpu->regs.sp, HIGH8(cpu->regs.pc));
    cpu->cycles += 3;
    cpu->regs.sp--;
    cpu_write(cpu, cpu->regs.sp, LOW8(cpu->regs.pc));
    cpu->cycles += 3;
    cpu->regs.pc = offset;
}


/* Input and Output Group */

/* OUT (n), a */
void out_na_a(Z80_t *cpu)
{
    // FIXME: no clue how timings or anything for this shite work
    cpu->cycles += 4;
    cpu->regs.pc++;
    uint8_t l = cpu_read(cpu, cpu->regs.pc);
    cpu->cycles += 3;
    cpu->regs.pc++;
    uint16_t addr = MAKE16(l, cpu->regs.main.a);
    cpu_out(cpu, addr, cpu->regs.main.a);
    cpu->cycles += 4;
}

void out_c_r(Z80_t *cpu, uint8_t value)
{
    cpu->cycles += 4;
    cpu->regs.pc++;
    cpu_out(cpu, cpu->regs.main.bc, value);
    cpu->cycles += 4;
}

void in_r_c(Z80_t *cpu, uint8_t *dest)
{
    cpu->cycles += 4;
    cpu->regs.pc++;
    uint8_t value = cpu_in(cpu, cpu->regs.main.bc);
    if (dest) *dest = value; 
    cpu->cycles += 4;
} 

void in_a_na(Z80_t *cpu)
{
    cpu->cycles += 4;
    cpu->regs.pc++;
    uint8_t l = cpu_read(cpu, cpu->regs.pc);
    cpu->cycles += 3;
    cpu->regs.pc++;
    uint16_t addr = MAKE16(l, cpu->regs.main.a);
    uint8_t value = cpu_in(cpu, addr);
    cpu->regs.main.a = value;
    cpu->cycles += 4;

    cpu->regs.main.flags.s = value & (1<<7);
    cpu->regs.main.flags.z = !value;
    cpu->regs.main.flags.h = 0;
    cpu->regs.main.flags.pv = get_parity(value);
    cpu->regs.main.flags.n = 0;
} 


/* CPU Control Group */

void nop(Z80_t *cpu)
{
    cpu->cycles += 4;
    cpu->regs.pc++;
}

void halt(Z80_t *cpu)
{
    cpu->cycles += 4;
    cpu->halted = true;
}

void di(Z80_t *cpu)
{
    cpu->regs.iff1 = 0;
    cpu->regs.iff2 = 0;
    cpu->cycles += 4;
    cpu->regs.pc++;
}

void ei(Z80_t *cpu)
{
    cpu->regs.iff1 = 1;
    cpu->regs.iff2 = 1;
    cpu->cycles += 4;
    cpu->regs.pc++;

    cpu->last_ei = true;
}

/* DD/FD handler */

void ddfd(Z80_t *cpu, bool is_iy)
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
    cpu->regs.im = 0;
    cpu->regs.pc = 0;

    cpu->error = 0;
    cpu->halted = 0;
    cpu->last_ei = 0;
    cpu->interrupt_pending = false;
}

void do_ed(Z80_t *cpu)
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

    default:
        // FIXME: HACK WHILE NOT ALL OPS ARE IMPLEMENTED
        if (op < 0x40 
        || (op >= 0xA0 && (op & 4))
        || (op >= 0x80 && op < 0xA0)) {
            nop(cpu);
        } else {
            cpu->regs.pc--;
            cpu->cycles -= 4;
            print_regs(cpu);
            dlog(LOG_ERR, "unimplemented opcode ED %02X at %04X", op, cpu->regs.pc);
            cpu->error = 1;
        }
    }
}

/* For bit instructions, I've decided to handle partial decoding 
 * for simplicity, as the opcode table is quite orthogonal. */
void do_cb(Z80_t *cpu)
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
    uint8_t op_partial = op >> 3;

    if (reg == 6) { // (hl)
        switch (op_partial) 
        {
            case 0x00: rlc_rra(cpu, cpu->regs.main.hl); break;
            case 0x01: rrc_rra(cpu, cpu->regs.main.hl); break;
            case 0x02: rl_rra(cpu, cpu->regs.main.hl); break;
            case 0x03: rr_rra(cpu, cpu->regs.main.hl); break;
            case 0x04: sla_rra(cpu, cpu->regs.main.hl); break;
            case 0x05: sra_rra(cpu, cpu->regs.main.hl); break;
            case 0x06: sll_rra(cpu, cpu->regs.main.hl); break;
            case 0x07: srl_rra(cpu, cpu->regs.main.hl); break;
            // bit
            case 0x08: bit_rra(cpu, cpu->regs.main.hl, 0); break;
            case 0x09: bit_rra(cpu, cpu->regs.main.hl, 1); break;
            case 0x0A: bit_rra(cpu, cpu->regs.main.hl, 2); break;
            case 0x0B: bit_rra(cpu, cpu->regs.main.hl, 3); break;
            case 0x0C: bit_rra(cpu, cpu->regs.main.hl, 4); break;
            case 0x0D: bit_rra(cpu, cpu->regs.main.hl, 5); break;
            case 0x0E: bit_rra(cpu, cpu->regs.main.hl, 6); break;
            case 0x0F: bit_rra(cpu, cpu->regs.main.hl, 7); break;
            // res
            case 0x10: res_rra(cpu, cpu->regs.main.hl, 0); break;
            case 0x11: res_rra(cpu, cpu->regs.main.hl, 1); break;
            case 0x12: res_rra(cpu, cpu->regs.main.hl, 2); break;
            case 0x13: res_rra(cpu, cpu->regs.main.hl, 3); break;
            case 0x14: res_rra(cpu, cpu->regs.main.hl, 4); break;
            case 0x15: res_rra(cpu, cpu->regs.main.hl, 5); break;
            case 0x16: res_rra(cpu, cpu->regs.main.hl, 6); break;
            case 0x17: res_rra(cpu, cpu->regs.main.hl, 7); break;
            // set
            case 0x18: set_rra(cpu, cpu->regs.main.hl, 0); break;
            case 0x19: set_rra(cpu, cpu->regs.main.hl, 1); break;
            case 0x1A: set_rra(cpu, cpu->regs.main.hl, 2); break;
            case 0x1B: set_rra(cpu, cpu->regs.main.hl, 3); break;
            case 0x1C: set_rra(cpu, cpu->regs.main.hl, 4); break;
            case 0x1D: set_rra(cpu, cpu->regs.main.hl, 5); break;
            case 0x1E: set_rra(cpu, cpu->regs.main.hl, 6); break;
            case 0x1F: set_rra(cpu, cpu->regs.main.hl, 7); break;
        }
    } else { // bcdehla
        uint8_t *regptr = regs[reg];

        switch (op_partial) 
        {
            case 0x00: rlc_r(cpu, regptr); break;
            case 0x01: rrc_r(cpu, regptr); break;
            case 0x02: rl_r(cpu, regptr); break;
            case 0x03: rr_r(cpu, regptr); break;
            case 0x04: sla_r(cpu, regptr); break;
            case 0x05: sra_r(cpu, regptr); break;
            case 0x06: sll_r(cpu, regptr); break;
            case 0x07: srl_r(cpu, regptr); break;
            // bit
            case 0x08: bit_r(cpu, *regptr, 0); break;
            case 0x09: bit_r(cpu, *regptr, 1); break;
            case 0x0A: bit_r(cpu, *regptr, 2); break;
            case 0x0B: bit_r(cpu, *regptr, 3); break;
            case 0x0C: bit_r(cpu, *regptr, 4); break;
            case 0x0D: bit_r(cpu, *regptr, 5); break;
            case 0x0E: bit_r(cpu, *regptr, 6); break;
            case 0x0F: bit_r(cpu, *regptr, 7); break;
            // res
            case 0x10: res_r(cpu, regptr, 0); break;
            case 0x11: res_r(cpu, regptr, 1); break;
            case 0x12: res_r(cpu, regptr, 2); break;
            case 0x13: res_r(cpu, regptr, 3); break;
            case 0x14: res_r(cpu, regptr, 4); break;
            case 0x15: res_r(cpu, regptr, 5); break;
            case 0x16: res_r(cpu, regptr, 6); break;
            case 0x17: res_r(cpu, regptr, 7); break;
            // set
            case 0x18: set_r(cpu, regptr, 0); break;
            case 0x19: set_r(cpu, regptr, 1); break;
            case 0x1A: set_r(cpu, regptr, 2); break;
            case 0x1B: set_r(cpu, regptr, 3); break;
            case 0x1C: set_r(cpu, regptr, 4); break;
            case 0x1D: set_r(cpu, regptr, 5); break;
            case 0x1E: set_r(cpu, regptr, 6); break;
            case 0x1F: set_r(cpu, regptr, 7); break;
        }
    }
}

void do_ddfd_cb(Z80_t *cpu, uint16_t *ii)
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

    cpu->cycles += 4;
    cpu->regs.pc++;
    uint8_t op = cpu_read(cpu, cpu->regs.pc);
    inc_refresh(cpu);

    uint8_t reg = op & 7; // op & 0b111
    uint8_t *regptr = regs[reg];

    uint8_t op_partial = op >> 3;

    switch (op_partial)
    {
        case 0x00: rlc_iid_r(cpu, addr, regptr); break;
        case 0x01: rrc_iid_r(cpu, addr, regptr); break;
        case 0x02: rl_iid_r(cpu, addr, regptr); break;
        case 0x03: rr_iid_r(cpu, addr, regptr); break;
        case 0x04: sla_iid_r(cpu, addr, regptr); break;
        case 0x05: sra_iid_r(cpu, addr, regptr); break;
        case 0x06: sll_iid_r(cpu, addr, regptr); break;
        case 0x07: srl_iid_r(cpu, addr, regptr); break;
        // bit
        case 0x08: bit_rra(cpu, addr, 0); break;
        case 0x09: bit_rra(cpu, addr, 1); break;
        case 0x0A: bit_rra(cpu, addr, 2); break;
        case 0x0B: bit_rra(cpu, addr, 3); break;
        case 0x0C: bit_rra(cpu, addr, 4); break;
        case 0x0D: bit_rra(cpu, addr, 5); break;
        case 0x0E: bit_rra(cpu, addr, 6); break;
        case 0x0F: bit_rra(cpu, addr, 7); break;
        // res
        case 0x10: res_iid_r(cpu, addr, regptr, 0); break;
        case 0x11: res_iid_r(cpu, addr, regptr, 1); break;
        case 0x12: res_iid_r(cpu, addr, regptr, 2); break;
        case 0x13: res_iid_r(cpu, addr, regptr, 3); break;
        case 0x14: res_iid_r(cpu, addr, regptr, 4); break;
        case 0x15: res_iid_r(cpu, addr, regptr, 5); break;
        case 0x16: res_iid_r(cpu, addr, regptr, 6); break;
        case 0x17: res_iid_r(cpu, addr, regptr, 7); break;
        // set
        case 0x18: set_iid_r(cpu, addr, regptr, 0); break;
        case 0x19: set_iid_r(cpu, addr, regptr, 1); break;
        case 0x1A: set_iid_r(cpu, addr, regptr, 2); break;
        case 0x1B: set_iid_r(cpu, addr, regptr, 3); break;
        case 0x1C: set_iid_r(cpu, addr, regptr, 4); break;
        case 0x1D: set_iid_r(cpu, addr, regptr, 5); break;
        case 0x1E: set_iid_r(cpu, addr, regptr, 6); break;
        case 0x1F: set_iid_r(cpu, addr, regptr, 7); break;
    }
}

/* FD/DD are basically "instructions" that tell the CPU 
 * "use IY/IX instead of HL for the next instruction".
 * there's some funky behavior associated with that i Do Not wanna emulate rn.
 * i also don't bother implementing illegal/duplicate opcodes */
void do_ddfd(Z80_t *cpu, bool is_iy)
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
    case 0x86: add_a_iid(cpu, *ii); break;
    case 0x8E: adc_a_iid(cpu, *ii); break;
    case 0x96: sub_iid(cpu, *ii); break;
    case 0x9E: sbc_a_iid(cpu, *ii); break;
    case 0xA6: and_iid(cpu, *ii); break;
    case 0xAE: xor_iid(cpu, *ii); break;
    case 0xB6: or_iid(cpu, *ii); break;
    case 0xBE: cp_iid(cpu, *ii); break;

    // non (ii+d) alo
    // add a
    case 0x80: add_a_r(cpu, cpu->regs.main.b); break;
    case 0x81: add_a_r(cpu, cpu->regs.main.c); break;
    case 0x82: add_a_r(cpu, cpu->regs.main.d); break;
    case 0x83: add_a_r(cpu, cpu->regs.main.e); break;
    case 0x84: add_a_r(cpu, *ih); break;
    case 0x85: add_a_r(cpu, *il); break;
    case 0x87: add_a_r(cpu, cpu->regs.main.a); break;
    // adc a
    case 0x88: adc_a_r(cpu, cpu->regs.main.b); break;
    case 0x89: adc_a_r(cpu, cpu->regs.main.c); break;
    case 0x8A: adc_a_r(cpu, cpu->regs.main.d); break;
    case 0x8B: adc_a_r(cpu, cpu->regs.main.e); break;
    case 0x8C: adc_a_r(cpu, *ih); break;
    case 0x8D: adc_a_r(cpu, *il); break;
    case 0x8F: adc_a_r(cpu, cpu->regs.main.a); break;
    // sub
    case 0x90: sub_r(cpu, cpu->regs.main.b); break;
    case 0x91: sub_r(cpu, cpu->regs.main.c); break;
    case 0x92: sub_r(cpu, cpu->regs.main.d); break;
    case 0x93: sub_r(cpu, cpu->regs.main.e); break;
    case 0x94: sub_r(cpu, *ih); break;
    case 0x95: sub_r(cpu, *il); break;
    case 0x97: sub_r(cpu, cpu->regs.main.a); break;
    // sbc a
    case 0x98: sbc_a_r(cpu, cpu->regs.main.b); break;
    case 0x99: sbc_a_r(cpu, cpu->regs.main.c); break;
    case 0x9A: sbc_a_r(cpu, cpu->regs.main.d); break;
    case 0x9B: sbc_a_r(cpu, cpu->regs.main.e); break;
    case 0x9C: sbc_a_r(cpu, *ih); break;
    case 0x9D: sbc_a_r(cpu, *il); break;
    case 0x9F: sbc_a_r(cpu, cpu->regs.main.a); break;
    // and
    case 0xA0: and_r(cpu, cpu->regs.main.b); break;
    case 0xA1: and_r(cpu, cpu->regs.main.c); break;
    case 0xA2: and_r(cpu, cpu->regs.main.d); break;
    case 0xA3: and_r(cpu, cpu->regs.main.e); break;
    case 0xA4: and_r(cpu, *ih); break;
    case 0xA5: and_r(cpu, *il); break;
    case 0xA7: and_r(cpu, cpu->regs.main.a); break;
    // xor
    case 0xA8: xor_r(cpu, cpu->regs.main.b); break;
    case 0xA9: xor_r(cpu, cpu->regs.main.c); break;
    case 0xAA: xor_r(cpu, cpu->regs.main.d); break;
    case 0xAB: xor_r(cpu, cpu->regs.main.e); break;
    case 0xAC: xor_r(cpu, *ih); break;
    case 0xAD: xor_r(cpu, *il); break;
    case 0xAF: xor_r(cpu, cpu->regs.main.a); break;
    // or
    case 0xB0: or_r(cpu, cpu->regs.main.b); break;
    case 0xB1: or_r(cpu, cpu->regs.main.c); break;
    case 0xB2: or_r(cpu, cpu->regs.main.d); break;
    case 0xB3: or_r(cpu, cpu->regs.main.e); break;
    case 0xB4: or_r(cpu, *ih); break;
    case 0xB5: or_r(cpu, *il); break;
    case 0xB7: or_r(cpu, cpu->regs.main.a); break;
    // cp
    case 0xB8: cp_r(cpu, cpu->regs.main.b); break;
    case 0xB9: cp_r(cpu, cpu->regs.main.c); break;
    case 0xBA: cp_r(cpu, cpu->regs.main.d); break;
    case 0xBB: cp_r(cpu, cpu->regs.main.e); break;
    case 0xBC: cp_r(cpu, *ih); break;
    case 0xBD: cp_r(cpu, *il); break;
    case 0xBF: cp_r(cpu, cpu->regs.main.a); break;

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

    default:
        print_regs(cpu);

        static const char ix_op[] = "DD";
        static const char iy_op[] = "FD";
        const char *prefix = is_iy ? iy_op : ix_op; 

        dlog(LOG_ERR, "unimplemented opcode %s %02X at %04X", prefix, op, cpu->regs.pc);
        cpu->error = 1;
    }
}

void do_opcode(Z80_t *cpu)
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
    case 0x02: ld_rra_r(cpu, cpu->regs.main.bc, cpu->regs.main.a); break;
    case 0x12: ld_rra_r(cpu, cpu->regs.main.de, cpu->regs.main.a); break;
    // ld a, (rr)
    case 0x0A: ld_r_rra(cpu, &cpu->regs.main.a, cpu->regs.main.bc); break;
    case 0x1A: ld_r_rra(cpu, &cpu->regs.main.a, cpu->regs.main.de); break;
    // ld a, (nn)
    case 0x3A: ld_r_nna(cpu, &cpu->regs.main.a); break;

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
    case 0x80: add_a_r(cpu, cpu->regs.main.b); break;
    case 0x81: add_a_r(cpu, cpu->regs.main.c); break;
    case 0x82: add_a_r(cpu, cpu->regs.main.d); break;
    case 0x83: add_a_r(cpu, cpu->regs.main.e); break;
    case 0x84: add_a_r(cpu, cpu->regs.main.h); break;
    case 0x85: add_a_r(cpu, cpu->regs.main.l); break;
    case 0x86: add_a_rra(cpu, cpu->regs.main.hl); break;
    case 0x87: add_a_r(cpu, cpu->regs.main.a); break;
    // adc a
    case 0x88: adc_a_r(cpu, cpu->regs.main.b); break;
    case 0x89: adc_a_r(cpu, cpu->regs.main.c); break;
    case 0x8A: adc_a_r(cpu, cpu->regs.main.d); break;
    case 0x8B: adc_a_r(cpu, cpu->regs.main.e); break;
    case 0x8C: adc_a_r(cpu, cpu->regs.main.h); break;
    case 0x8D: adc_a_r(cpu, cpu->regs.main.l); break;
    case 0x8E: adc_a_rra(cpu, cpu->regs.main.hl); break;
    case 0x8F: adc_a_r(cpu, cpu->regs.main.a); break;
    // sub
    case 0x90: sub_r(cpu, cpu->regs.main.b); break;
    case 0x91: sub_r(cpu, cpu->regs.main.c); break;
    case 0x92: sub_r(cpu, cpu->regs.main.d); break;
    case 0x93: sub_r(cpu, cpu->regs.main.e); break;
    case 0x94: sub_r(cpu, cpu->regs.main.h); break;
    case 0x95: sub_r(cpu, cpu->regs.main.l); break;
    case 0x96: sub_rra(cpu, cpu->regs.main.hl); break;
    case 0x97: sub_r(cpu, cpu->regs.main.a); break;
    // sbc a
    case 0x98: sbc_a_r(cpu, cpu->regs.main.b); break;
    case 0x99: sbc_a_r(cpu, cpu->regs.main.c); break;
    case 0x9A: sbc_a_r(cpu, cpu->regs.main.d); break;
    case 0x9B: sbc_a_r(cpu, cpu->regs.main.e); break;
    case 0x9C: sbc_a_r(cpu, cpu->regs.main.h); break;
    case 0x9D: sbc_a_r(cpu, cpu->regs.main.l); break;
    case 0x9E: sbc_a_rra(cpu, cpu->regs.main.hl); break;
    case 0x9F: sbc_a_r(cpu, cpu->regs.main.a); break;
    // and
    case 0xA0: and_r(cpu, cpu->regs.main.b); break;
    case 0xA1: and_r(cpu, cpu->regs.main.c); break;
    case 0xA2: and_r(cpu, cpu->regs.main.d); break;
    case 0xA3: and_r(cpu, cpu->regs.main.e); break;
    case 0xA4: and_r(cpu, cpu->regs.main.h); break;
    case 0xA5: and_r(cpu, cpu->regs.main.l); break;
    case 0xA6: and_rra(cpu, cpu->regs.main.hl); break;
    case 0xA7: and_r(cpu, cpu->regs.main.a); break;
    // xor
    case 0xA8: xor_r(cpu, cpu->regs.main.b); break;
    case 0xA9: xor_r(cpu, cpu->regs.main.c); break;
    case 0xAA: xor_r(cpu, cpu->regs.main.d); break;
    case 0xAB: xor_r(cpu, cpu->regs.main.e); break;
    case 0xAC: xor_r(cpu, cpu->regs.main.h); break;
    case 0xAD: xor_r(cpu, cpu->regs.main.l); break;
    case 0xAE: xor_rra(cpu, cpu->regs.main.hl); break;
    case 0xAF: xor_r(cpu, cpu->regs.main.a); break;
    // or
    case 0xB0: or_r(cpu, cpu->regs.main.b); break;
    case 0xB1: or_r(cpu, cpu->regs.main.c); break;
    case 0xB2: or_r(cpu, cpu->regs.main.d); break;
    case 0xB3: or_r(cpu, cpu->regs.main.e); break;
    case 0xB4: or_r(cpu, cpu->regs.main.h); break;
    case 0xB5: or_r(cpu, cpu->regs.main.l); break;
    case 0xB6: or_rra(cpu, cpu->regs.main.hl); break;
    case 0xB7: or_r(cpu, cpu->regs.main.a); break;
    // cp
    case 0xB8: cp_r(cpu, cpu->regs.main.b); break;
    case 0xB9: cp_r(cpu, cpu->regs.main.c); break;
    case 0xBA: cp_r(cpu, cpu->regs.main.d); break;
    case 0xBB: cp_r(cpu, cpu->regs.main.e); break;
    case 0xBC: cp_r(cpu, cpu->regs.main.h); break;
    case 0xBD: cp_r(cpu, cpu->regs.main.l); break;
    case 0xBE: cp_rra(cpu, cpu->regs.main.hl); break;
    case 0xBF: cp_r(cpu, cpu->regs.main.a); break;

    // add/adc/sub/sbc/and/xor/or/cp n
    case 0xC6: add_a_n(cpu); break;
    case 0xCE: adc_a_n(cpu); break;
    case 0xD6: sub_n(cpu); break;
    case 0xDE: sbc_a_n(cpu); break;
    case 0xE6: and_n(cpu); break;
    case 0xEE: xor_n(cpu); break;
    case 0xF6: or_n(cpu); break;
    case 0xFE: cp_n(cpu); break;

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
    case 0xC3: jp_nn(cpu); break;
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

    if (cpu->interrupt_pending && cpu_can_process_interrupts(cpu)) {
        if (cpu->halted) {
            cpu->halted = false;
            cpu->regs.pc++;
        }
        // FIXME: COMPLETELY INACCURATE TIMINGS FOR EVERYTHING
        switch (cpu->regs.im)
        {
        case 0: // IM 0 is irrel on speccy, don't care
        case 1:
            cpu->regs.iff1 = 0;
            cpu->regs.iff2 = 0;
            cpu->cycles += 5;
            cpu->regs.sp--;
            cpu_write(cpu, cpu->regs.sp, HIGH8(cpu->regs.pc));
            cpu->cycles += 3;
            cpu->regs.sp--;
            cpu_write(cpu, cpu->regs.sp, LOW8(cpu->regs.pc));
            cpu->cycles += 3;
            cpu->regs.pc = 0x38;
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
            cpu->cycles += 3;
            break;
        }
    } else {
        cpu->last_ei = false;
        switch (cpu->prefix_state) 
        {
            case STATE_DD: do_ddfd(cpu, false); break;
            case STATE_FD: do_ddfd(cpu, true); break;
            default: do_opcode(cpu); break;
        }
    }

    if (cpu->halted) {
        cpu_read(cpu, cpu->regs.pc+1);
        cpu->cycles += 4;
    }

    cpu->interrupt_pending = false;

    return cpu->cycles - cyc_old;
}