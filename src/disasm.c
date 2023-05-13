#include "disasm.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *t_r[] = {
    "b", "c", "d", "e", "h", "l", "(hl)", "a"
};

static const char *t_rp[] = {
    "bc", "de", "hl", "sp"
};

static const char *t_rp2[] = {
    "bc", "de", "hl", "af"
};

static const char *t_cc[] = {
    "nz", "z", "nc", "c", "po", "pe", "p", "m"
};

static const char *t_alu[] = {
    "add a,", "adc a,", "sub", "sbc a,", "and", "xor", "or", "cp"
};

static const char *t_sro[] = {
    "rlc", "rrc", "rl", "rr", "sla", "sra", "sll", "srl"
};

static const char *t_blk[] = {
    "ldi",  "cpi",  "ini",  "outi",
    "ldd",  "cpd",  "ind",  "outd",
    "ldir", "cpir", "inir", "otir",
    "lddr", "cpdr", "indr", "otdr",
};

static const char *t_im[] = { 
    "0", "0*", "1", "2", "0*", "0*", "1*", "2*"
};

static const char *t_x1_z1_q[] = { 
    "ret", "exx", "jp %s", "ld sp, %s"
};

static const char *t_x3_z3[] = { 
    "ex (sp), hl", "ex de", "di", "ei"
};

static const char *t_x0_z7[] = { 
    "rlca", "rrca", "rla", "rra", "daa", "cpl", "scf", "ccf"
};

static const char *t_x0_z2[] = { 
    "ld (bc), a",  "ld a, (bc)",
    "ld (de), a",  "ld a, (de)",
    "ld (%s), %s", "ld %s, (%s)",
    "ld (%s), a",  "ld a, (%s)",
};

static const char *ed_x1_z7[] = {
    "ld i, a", "ld r, a",
    "ld a, i", "ld a, r",
    "rrd", "rld",
    "nop*", "nop*" 
};

static const char *tt_r(uint8_t **data, int i, int prefix)
{
    const char *iid[] = { "(ix%+hhd)", "(iy%+hhd)" };
    const char *ih[]  = { "ixh", "iyh" };
    const char *il[]  = { "ixl", "iyl" };

    static char buf[128];
    if (prefix) {
        if (i == 6) {
            int8_t offset = **data;
            *data += 1;
            snprintf(buf, sizeof(buf), iid[prefix-1], offset);
            return buf;
        } else if (i == 4) {
            return ih[prefix-1];
        } else if (i == 5) {
            return il[prefix-1];
        }
    }

    return t_r[i];
}

static const char *tt_rp(int i, int prefix)
{
    if (i == 2 && prefix) {
        return prefix == 1 ? "ix" : "iy";
    }

    return t_rp[i];
}

static const char *tt_rp2(int i, int prefix)
{
    if (i == 2 && prefix) {
        return prefix == 1 ? "ix" : "iy";
    }

    return t_rp2[i];
}

/* should potentially handle labels and shite
 * NON REENTRANT */
static char *get_paddr_str(uint8_t *p_addr)
{
    uint16_t a = p_addr[1] << 8 | p_addr[0];
    static char buf[128];
    snprintf(buf, sizeof(buf), "$%04x", a);
    return buf;
}

static char *get_addr_str(uint16_t addr)
{
    static char buf[128];
    snprintf(buf, sizeof(buf), "$%04x", addr);
    return buf;
}

static char *get_byte_str(uint8_t b)
{
    static char buf[128];
    snprintf(buf, sizeof(buf), "$%02x", b);
    return buf;
}

static int prefix_cb(uint8_t *data, char *buf, size_t buflen)
{
    uint8_t op = *data;

    int x = op >> 6;
    int y = (op >> 3) & 7;
    int z = op & 7;

    switch (x)
    {
    case 0: snprintf(buf, buflen, "%s %s", t_sro[y], t_r[z]); break;
    case 1: snprintf(buf, buflen, "bit %d, %s", y, t_r[z]); break;
    case 2: snprintf(buf, buflen, "res %d, %s", y, t_r[z]); break;
    case 3: snprintf(buf, buflen, "set %d, %s", y, t_r[z]); break;
    }

    return 1;
}

static int prefix_ddfd_cb(uint8_t *data, char *buf, size_t buflen, int prefix)
{
    uint8_t op = *data;
    data++;

    int x = op >> 6;
    int y = (op >> 3) & 7;
    int z = op & 7;

    static const char *r[] = {
        ", b", ", c", ", d", ", e", ", h", ", l", "", ", a"
    };

    switch (x)
    {
    case 0: snprintf(buf, buflen, "%s %s%s", t_sro[y], tt_r(&data, 6, prefix), r[z]); break;
    case 1: snprintf(buf, buflen, "bit %d, %s", y, tt_r(&data, 6, prefix)); break;
    case 2: snprintf(buf, buflen, "res %d, %s%s", y, tt_r(&data, 6, prefix), r[z]); break;
    case 3: snprintf(buf, buflen, "set %d, %s%s", y, tt_r(&data, 6, prefix), r[z]); break;
    }

    return 2;
}

static int prefix_ed(uint8_t *data, char *buf, size_t buflen)
{
    uint8_t op = *data;
    data++;

    int x = op >> 6;
    int y = (op >> 3) & 7;
    int z = op & 7;
    int q = op & (1<<3);
    int p = (op >> 4) & 3;

    const char *fmt = "UNHANDLED";
    const char *s1 = NULL;
    const char *s2 = NULL;

    int len = 1;

    switch (x)
    {
    case 0:
    case 3: fmt = "nop*"; break;
    case 2: fmt = (z<4 && y>=4) ? t_blk[(y-4)*4 + z] : "nop*"; break;
    case 1:
        switch (z)
        {
        case 0: fmt = (y != 6) ? "in %s, (c)" : "in (c)";      s1 = t_r[z]; break;
        case 1: fmt = (y != 6) ? "out (c), %s" : "out (c), 0"; s1 = t_r[z]; break;
        case 2: fmt = q ? "adc hl, %s" : "sbc hl, %s";         s1 = t_rp[p]; break;
        case 3:
            if (q) { fmt = "ld %s, (%s)"; s1 = t_rp[p]; s2 = get_paddr_str(data); }
            else   { fmt = "ld (%s), %s"; s1 = get_paddr_str(data); s2 = t_rp[p]; }
            len += 2;
            break;
        case 4: fmt = (y == 0) ? "neg"  : "neg*"; break;
        case 5: fmt = (y == 1) ? "reti" : "retn"; break;
        case 6: fmt = "im %s"; s1 = t_im[y]; break;
        case 7: fmt = ed_x1_z7[y]; break;
        }
    }

    snprintf(buf, buflen, fmt, s1, s2);
    return len;
}

static char *opcode(uint8_t *data, int *len, uint16_t pc, int prefix)
{
    uint8_t *dorg = data;
    uint8_t op = *data;
    data++;

    int x = op >> 6;
    int y = (op >> 3) & 7;
    int z = op & 7;
    int q = op & (1<<3);
    int p = (op >> 4) & 3;

    char buf[128] = "UNHANDLED";
    const size_t buflen = sizeof(buf);

    const char *fmt = "UNHANDLED";
    const char *s1 = NULL;
    const char *s2 = NULL;
    int noprint = 0;

    switch (x)
    {
    case 0:
        switch (z)
        {
        case 0:
            switch (y)
            {
            case 0: fmt = "nop"; break;
            case 1: fmt = "ex af"; break;
            case 2: fmt = "djnz %s"; s1 = get_addr_str(pc+2 + (int8_t)*data); data++; break;
            case 3: fmt = "jr %s";   s1 = get_addr_str(pc+2 + (int8_t)*data); data++; break;
            default:
                fmt = "jr %s, %s"; 
                s1 = t_cc[y-4]; s2 = get_addr_str(pc+2 + (int8_t)*data);
                data++;
                break;
            }
            break;
        case 1:
            if (q) { fmt = "add %s, %s"; s1 = tt_rp(2, prefix); s2 = tt_rp(p, prefix); } 
            else   { fmt = "ld %s, %s",  s1 = tt_rp(p, prefix); s2 = get_paddr_str(data); data += 2; }
            break;
        case 2:
            fmt = t_x0_z2[y];
            if (y >= 4) {
                if (y == 5) {
                    s1 = tt_rp(2, prefix);
                    s2 = get_paddr_str(data);
                } else {
                    s1 = get_paddr_str(data);
                    s2 = tt_rp(2, prefix);
                }
                data += 2;
            }
            break;
        case 3: fmt = q ? "dec %s" : "inc %s"; s1 = tt_rp(p, prefix);break;
        case 4: fmt = "inc %s"; s1 = tt_r(&data, y, prefix); break;
        case 5: fmt = "dec %s"; s1 = tt_r(&data, y, prefix); break;
        case 6: fmt = "ld %s, %s", s1 = tt_r(&data, y, prefix); s2 = get_byte_str(*data); data++; break;
        case 7: fmt = t_x0_z7[y]; break;
        }
        break;
    case 1: 
        if (op == 0x76) {
            fmt = "halt";
        } else { 
            fmt = "ld %s, %s";
            if (y == 6) {
                s1 = tt_r(&data, y, prefix);
                s2 = t_r[z];
            } else if (z == 6) {
                s1 = t_r[y];
                s2 = tt_r(&data, z, prefix);
            } else {
                s1 = tt_r(&data, y, prefix);
                s2 = tt_r(&data, z, prefix);
            }
        }
        break;
    case 2: fmt = "%s %s"; s1 = t_alu[y]; s2 = tt_r(&data, z, prefix); break;
    case 3:
        switch (z)
        {
        case 0: fmt = "ret %s"; s1 = t_cc[y]; break;
        case 1:
            if (q) { fmt = t_x1_z1_q[p]; s1 = tt_rp(2, prefix); }
            else   { fmt = "pop %s"; s1 = tt_rp2(p, prefix); }
            break;
        case 2: fmt = "jp %s, %s"; s1 = t_cc[y]; s2 = get_paddr_str(data); data += 2; break;
        case 3:
            switch (y)
            {
            case 0: fmt = "jp %s"; s1 = get_paddr_str(data); data += 2; break;
            case 1:
                if (prefix) {
                    data += prefix_ddfd_cb(data, buf, buflen,prefix); 
                } else {
                    data += prefix_cb(data, buf, buflen); 
                }
                noprint = 1;
                break;
            case 2: fmt = "out (%s), a"; s1 = get_byte_str(*data); data++; break;
            case 3: fmt = "in a, (%s)";  s1 = get_byte_str(*data); data++; break;
            default:
                fmt = t_x3_z3[y-4];
            }
            break;
        case 4: fmt = "call %s, %s"; s1 = t_cc[y]; s2 = get_paddr_str(data); data += 2; break;
        case 5:
            if (q) {
                switch (p)
                {
                case 0: fmt = "call %s"; s1 = get_byte_str(*data); data += 2; break;
                case 1: // dd
                    if (prefix) {
                        fmt = "nop*";
                    } else {
                        return opcode(data, len, pc, 1);
                    }
                    break;
                case 2:
                    if (prefix) {
                        fmt = "nop*";
                        data--;
                    } else {
                        data += prefix_ed(data, buf, buflen); noprint = 1;
                    }
                    break;
                case 3: // fd
                    if (prefix) {
                        fmt = "nop*";
                    } else {
                        return opcode(data, len, pc, 2);
                    }
                    break;
                }
            } else {
                fmt = "push %s"; s1 = tt_rp2(p, prefix);
            }
            break;
        case 6: fmt = "%s %s"; s1 = t_alu[y]; s2 = get_byte_str(*data); data++; break;
        case 7: fmt = "rst %s"; s1 = get_byte_str(y*8); break;
        }
    }

    if (!noprint) snprintf(buf, buflen, fmt, s1, s2);

    *len = data - dorg + !(!prefix);

    size_t slen = strlen(buf) + 1;
    char *str = malloc(slen);
    if (str == NULL) {
        return NULL;
    }
    memcpy(str, buf, slen-1);
    str[slen-1] = 0;

    return str;
}

char *disassemble_opcode(uint8_t *data, int *len, uint16_t pc)
{
    return opcode(data, len, pc, 0);
}
