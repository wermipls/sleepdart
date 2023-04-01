#include "z80.h"
#include "memory.h"
#include "log.h"

Z80_t cpu;

/* helpers */

#define MASK_FLAG_XY    ((1<<3) | (1<<5))
#define MAKE16(L, H)    (L | (H << 8))
#define LOW8(HL)        (HL & 255)
#define HIGH8(HL)       (HL >> 8)

void print_regs(Z80_t *cpu)
{
    dlog(LOG_INFO, "cycles: %d", cpu->cycles);
    dlog(LOG_INFO, "  %02X %02X %02X %02X %02X",
                   memory_bus_peek(cpu->regs.pc-2),
                   memory_bus_peek(cpu->regs.pc-1),
                   memory_bus_peek(cpu->regs.pc),
                   memory_bus_peek(cpu->regs.pc+1),
                   memory_bus_peek(cpu->regs.pc+2));
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
    cpu->cycles += memory_read(addr, &value);
    return value;
}

static inline void cpu_write(Z80_t *cpu, uint16_t addr, uint8_t value)
{
    cpu->cycles += memory_write(addr, value);
}

static inline uint8_t cpu_in(Z80_t *cpu, uint16_t addr)
{
    // FIXME: placeholder until port i/o gets implemented
    // uint8_t value;
    // cpu->cycles += memory_read(addr, &value);
    return 0xFA;
}

static inline void cpu_out(Z80_t *cpu, uint16_t addr, uint8_t value)
{
    // FIXME: placeholder until port i/o gets implemented
    // cpu->cycles += memory_write(addr, value);
}

static inline bool get_parity(uint8_t value)
{
    // FIXME: untested HACK
    value = value ^ (value >> 4);
    value = value ^ (value >> 2);
    value = value ^ (value >> 1);
    return !(value & 1);
}

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

/* LD I, A */
void ld_i_a(Z80_t *cpu)
{
    cpu->regs.i = cpu->regs.main.a;
    cpu->regs.pc++;
    cpu->cycles += 5;
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
    cpu_write(cpu, addr, LOW8(addr));
    cpu->cycles += 3;
    cpu_write(cpu, addr+1, HIGH8(addr));
    cpu->cycles += 3;
}

void ld_sp_rr(Z80_t *cpu, uint16_t value)
{
    cpu->cycles += 6;
    cpu->regs.pc++;
    cpu->regs.sp = value;
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

/* LDI/LDD */
void ldx(Z80_t *cpu, int8_t increment)
{
    // FIXME: fuck flags
    cpu->cycles += 4;
    uint8_t value = cpu_read(cpu, cpu->regs.main.hl);
    cpu->cycles += 3;
    cpu_write(cpu, cpu->regs.main.de, value);
    // FIXME: unaccurate contention timings 
    cpu->cycles += 5;
    cpu->regs.main.hl += increment;
    cpu->regs.main.de += increment;
    cpu->regs.main.bc--;
}

/* LDIR/LDDR */
void ldxr(Z80_t *cpu, int8_t increment)
{
    // FIXME: fuck flags
    cpu->cycles += 4;
    uint8_t value = cpu_read(cpu, cpu->regs.main.hl);
    cpu->cycles += 3;
    cpu_write(cpu, cpu->regs.main.de, value);
    // FIXME: unaccurate contention timings 
    cpu->cycles += 5;
    cpu->regs.main.hl += increment;
    cpu->regs.main.de += increment;
    cpu->regs.main.bc--;

    if (cpu->regs.main.bc != 0) {
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
    cpu->regs.main.flags.n = 0;
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

/* ADD helper */
static inline uint8_t add8(Z80_t *cpu, uint8_t value)
{
    uint8_t a = cpu->regs.main.a;
    uint8_t result = a + value;
    cpu->regs.main.f &= ~MASK_FLAG_XY;
    cpu->regs.main.f |= (value & MASK_FLAG_XY);
    cpu->regs.main.flags.s = result & (1<<7);
    cpu->regs.main.flags.z = !result;
    uint8_t operands_same_sign = !((a ^ value) & (1<<7));
    if (operands_same_sign) {
        cpu->regs.main.flags.pv = !(!((result ^ value) & (1<<7)));
    } else {
        cpu->regs.main.flags.pv = 0;
    }
    // FIXME: dunno about correctness of (half) carry or PV
    cpu->regs.main.flags.h = ((a & 0x0F) + (value & 0x0F)) > 0x0F;
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
    cpu->regs.pc++;
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
    // FIXME: those flags are DEFINITELY wrong and i cba for now
    uint8_t operands_same_sign = !((a ^ value) & (1<<7));
    if (operands_same_sign) {
        cpu->regs.main.flags.pv = !(!((result ^ value) & (1<<7)));
    } else {
        cpu->regs.main.flags.pv = 0;
    }
    cpu->regs.main.flags.h = ((a & 0x0F) + (value & 0x0F)) > 0x0F;
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
    cpu->regs.pc++;
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
    // FIXME: those flags are DEFINITELY wrong and i cba for now
    uint8_t operands_same_sign = !((a ^ value) & (1<<7));
    if (operands_same_sign) {
        cpu->regs.main.flags.pv = 0;
    } else {
        cpu->regs.main.flags.pv = !(!((result ^ value) & (1<<7)));
    }
    cpu->regs.main.flags.h = ((a & 0xF0) - (value & 0xF0)) < 0xF0;
    cpu->regs.main.flags.c = ((uint16_t)a - (uint16_t)value) > 255;
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
    cpu->regs.pc++;
    cpu->regs.main.a = sbc8(cpu, value);
}

/* SUB helper */
static inline uint8_t sub8(Z80_t *cpu, uint8_t value)
{
    uint8_t a = cpu->regs.main.a;
    uint8_t result = a - value;
    cpu->regs.main.f &= ~MASK_FLAG_XY;
    cpu->regs.main.f |= (result & MASK_FLAG_XY);
    cpu->regs.main.flags.s = result & (1<<7);
    cpu->regs.main.flags.z = !result;
    uint8_t operands_same_sign = !((a ^ value) & (1<<7));
    if (operands_same_sign) {
        cpu->regs.main.flags.pv = 0;
    } else {
        cpu->regs.main.flags.pv = !(!((result ^ value) & (1<<7)));
    }
    // FIXME: dunno about correctness of (half) carry or PV
    cpu->regs.main.flags.h = ((a & 0xF0) - (value & 0xF0)) < 0xF0;
    cpu->regs.main.flags.c = ((uint16_t)a - (uint16_t)value) > (uint16_t)255;
    cpu->regs.main.flags.n = 1;
    return result;
}

/* SUB r */
void sub_r(Z80_t *cpu, uint8_t value)
{
    cpu->cycles += 4;
    cpu->regs.pc++;
    cpu->regs.main.a = sub8(cpu, value);
}

/* SUB n */
void sub_n(Z80_t *cpu)
{
    cpu->cycles += 4;
    cpu->regs.pc++;
    uint8_t value = cpu_read(cpu, cpu->regs.pc);
    cpu->cycles += 3;
    cpu->regs.pc++;
    cpu->regs.main.a = sub8(cpu, value);
}

/* SUB (rr) */
void sub_rra(Z80_t *cpu, uint16_t addr)
{
    cpu->cycles += 4;
    cpu->regs.pc++;
    uint8_t value = cpu_read(cpu, addr);
    cpu->cycles += 3;
    cpu->regs.pc++;
    cpu->regs.main.a = sub8(cpu, value);
}

/* CP r */
void cp_r(Z80_t *cpu, uint8_t value)
{
    cpu->cycles += 4;
    cpu->regs.pc++;
    sub8(cpu, value);
}

/* CP n */
void cp_n(Z80_t *cpu)
{
    cpu->cycles += 4;
    cpu->regs.pc++;
    uint8_t value = cpu_read(cpu, cpu->regs.pc);
    cpu->cycles += 3;
    cpu->regs.pc++;
    sub8(cpu, value);
}

/* CP (rr) */
void cp_rra(Z80_t *cpu, uint16_t addr)
{
    cpu->cycles += 4;
    cpu->regs.pc++;
    uint8_t value = cpu_read(cpu, addr);
    cpu->cycles += 3;
    cpu->regs.pc++;
    sub8(cpu, value);
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

/* General-Purpose Arithmetic and CPU Control Groups */

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
    *dest = (uint16_t)result;
    cpu->regs.main.flags.h = 0; // FIXME: placeholder, should be carry from bit 11
    cpu->regs.main.flags.n = 0;
    cpu->regs.main.flags.c = result & (1<<16); // hmm thats kinda stupid
    cpu->cycles += 11;
    cpu->regs.pc++;
}

void adc_rr_rr(Z80_t *cpu, uint16_t *dest, uint16_t value)
{
    uint32_t result = (uint32_t)*dest + value + cpu->regs.main.flags.c;
    *dest = (uint16_t)result;
    cpu->regs.main.flags.s = result & (1<<15);
    cpu->regs.main.flags.z = !(result & 0xFF);
    cpu->regs.main.flags.h = 0; // FIXME: placeholder, should be carry from bit 11
    cpu->regs.main.flags.pv = 0; // FIXME: placeholder, should be set if overflow
    cpu->regs.main.flags.n = 0;
    cpu->regs.main.flags.c = result & (1<<16); // hmm thats kinda stupid
    cpu->cycles += 11;
    cpu->regs.pc++;
}

void sbc_rr_rr(Z80_t *cpu, uint16_t *dest, uint16_t value)
{
    uint32_t result = (uint32_t)*dest - value - cpu->regs.main.flags.c;
    *dest = (uint16_t)result;
    cpu->regs.main.flags.s = result & (1<<15);
    cpu->regs.main.flags.z = !(result & 0xFF);
    cpu->regs.main.flags.h = 0; // FIXME: placeholder, should be carry from bit 11
    cpu->regs.main.flags.pv = 0; // FIXME: placeholder, should be set if overflow
    cpu->regs.main.flags.n = 1;
    cpu->regs.main.flags.c = result & (1<<31); // hmm thats kinda stupid
    cpu->cycles += 11;
    cpu->regs.pc++;
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
    if (cc) {
        cpu->regs.pc = MAKE16(l, h); // FIXME: figure out a nicer macro for this 
    } else {
        cpu->regs.pc++;
    }
    cpu->cycles += 3;
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

/* CPU Control Group */

void nop(Z80_t *cpu)
{
    cpu->cycles += 4;
    cpu->regs.pc++;
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
}

void do_ed(Z80_t *cpu)
{
    cpu->cycles += 4;
    cpu->regs.pc++;
    uint8_t op = cpu_read(cpu, cpu->regs.pc);

    switch (op)
    {
    case 0x47: ld_i_a(cpu); break;

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
    // im 0/1/2
    case 0x46: im(cpu, 0); break;
    case 0x56: im(cpu, 1); break;
    case 0x5E: im(cpu, 2); break;
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

    default:
        cpu->regs.pc--;
        cpu->cycles -= 4;
        print_regs(cpu);
        dlog(LOG_ERR, "unimplemented opcode ED %02X at %04X", op, cpu->regs.pc);
    }
}

/* FD/DD are basically "instructions" that tell the CPU 
 * "use IY/IX instead of HL for the next instruction".
 * there's some funky behavior associated with that i Do Not wanna emulate rn.
 * i also don't bother implementing illegal/duplicate opcodes */
void do_ddfd(Z80_t *cpu, bool is_iy)
{
    uint16_t *ii = is_iy ? &cpu->regs.iy : &cpu->regs.ix;

    uint8_t op = cpu_read(cpu, cpu->regs.pc);

    switch (op)
    {
    // ld iy, nn
    case 0x21: ld_rr_nn(cpu, ii); break;
    // inc iy
    case 0x23: inc_rr(cpu, ii); break;
    // dec iy
    case 0x2B: dec_rr(cpu, ii); break;
    // add iy, rr
    case 0x09: add_rr_rr(cpu, ii, cpu->regs.main.bc); break;
    case 0x19: add_rr_rr(cpu, ii, cpu->regs.main.de); break;
    case 0x29: add_rr_rr(cpu, ii, cpu->regs.iy); break;
    case 0x39: add_rr_rr(cpu, ii, cpu->regs.sp); break;

    // ld (nn), iy
    case 0x22: ld_nna_rr(cpu, *ii); break;
    // ld iy, (nn)
    case 0x2A: ld_rr_nna(cpu, ii); break;
    // ld sp, iy
    case 0xF9: ld_sp_rr(cpu, *ii); break;

    default:
        print_regs(cpu);

        static const char ix_op[] = "DD";
        static const char iy_op[] = "FD";
        const char *prefix = is_iy ? iy_op : ix_op; 

        dlog(LOG_ERR, "unimplemented opcode %s %02X at %04X", prefix, op, cpu->regs.pc);
    }

    cpu->prefix_state = STATE_NOPREFIX;
}

void do_opcode(Z80_t *cpu)
{
    // oppa gangnam style
    uint8_t op = cpu_read(cpu, cpu->regs.pc);

    switch (op)
    {
    // ld rr, nn
    case 0x01: ld_rr_nn(cpu, &cpu->regs.main.bc); break;
    case 0x11: ld_rr_nn(cpu, &cpu->regs.main.de); break;
    case 0x21: ld_rr_nn(cpu, &cpu->regs.main.hl); break;
    case 0x31: ld_rr_nn(cpu, &cpu->regs.sp); break;

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

    // ld (nn), hl
    case 0x22: ld_nna_rr(cpu, cpu->regs.main.hl); break;
    // ld hl, (nn)
    case 0x2A: ld_rr_nna(cpu, &cpu->regs.main.hl); break;
    // ld (nn), a
    case 0x32: ld_nna_a(cpu); break;
    // ld sp, hl
    case 0xF9: ld_sp_rr(cpu, cpu->regs.main.hl); break;

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

    // add/adc/sub/sbc/and/xor/or/cp n
    case 0xC6: add_a_n(cpu); break;
    case 0xD6: sub_n(cpu); break;
    case 0xFE: cp_n(cpu); break;

    // out (n), a
    case 0xD3: out_na_a(cpu); break;

    // exx
    case 0xD9: exx(cpu); break;
    // XD
    case 0xEB: ex_de_hl(cpu); break;

    // DI / EI
    case 0xF3: di(cpu); break;
    case 0xFB: ei(cpu); break;

    case 0x00: nop(cpu); break;

    case 0xED: do_ed(cpu); break;

    // IY/IX prefix
    case 0xDD: ddfd(cpu, false); break;
    case 0xFD: ddfd(cpu, true); break;

    default:
        print_regs(cpu);
        dlog(LOG_ERR, "unimplemented opcode %02X at %04X", op, cpu->regs.pc);
    }
}

int cpu_do_cycles(Z80_t *cpu)
{
    uint64_t cyc_old = cpu->cycles;

    switch (cpu->prefix_state) 
    {
        case STATE_DD: do_ddfd(cpu, false); break;
        case STATE_FD: do_ddfd(cpu, true); break;
        default: do_opcode(cpu); break;
    }

    return cpu->cycles - cyc_old;
}