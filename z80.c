#include "z80.h"
#include "memory.h"
#include "log.h"

Z80_t cpu;

/* helpers */

void print_regs(Z80_t *cpu)
{
    dlog(LOG_INFO, "cycles: %d", cpu->cycles);
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

static inline bool get_parity(uint8_t value)
{
    // FIXME: untested HACK
    value = value ^ (value >> 4);
    value = value ^ (value >> 2);
    value = value ^ (value >> 1);
    return value & 1;
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

/* 8-Bit Arithmetic Group */

/* AND helper */
static inline void and(Z80_t *cpu, uint8_t value)
{
    value &= cpu->regs.main.a;
    cpu->regs.main.a = value;
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

/* The Real Deal */

int cpu_do_cycles(Z80_t *cpu)
{
    uint64_t cyc_old = cpu->cycles;
    print_regs(cpu);

    // oppa gangnam style
    uint8_t op = cpu_read(cpu, cpu->regs.pc);

    switch (op)
    {
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

    // DI / EI
    case 0xF3: di(cpu); break;
    case 0xFB: ei(cpu); break;

    case 0x00: nop(cpu); break;

    default:
        dlog(LOG_ERR, "unimplemented opcode %02X at %04X", op, cpu->regs.pc);
    }

    return cpu->cycles - cyc_old;
}