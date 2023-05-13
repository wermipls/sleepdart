#include "disasm.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "memory.h"
#include "log.h"

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
    "ret", "exx", "jp hl", "ld sp, hl"
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
    "ld (%s), hl", "ld hl, (%s)",
    "ld (%s), a",  "ld a, (%s)",
};

static const char *ed_x1_z7[] = {
    "ld i, a", "ld r, a",
    "ld a, i", "ld a, r",
    "rrd", "rld",
    "nop*", "nop*" 
};

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

char *disassemble_opcode(uint8_t *data, int *len, uint16_t pc)
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

    switch (x)
    {
    case 0:
        switch (z)
        {
        case 0:
            switch (y)
            {
            case 0: strncpy(buf, "nop", buflen); break;
            case 1: strncpy(buf, "ex af", buflen); break;
            case 2:
                snprintf(buf, buflen, "djnz %s", get_addr_str(pc+2 + (int8_t)*data));
                data++;
                break;
            case 3:
                snprintf(buf, buflen, "jr %s", get_addr_str(pc+2 + (int8_t)*data));
                data++;
                break;
            default:
                snprintf(buf, buflen, "jr %s, %s", t_cc[y-4], get_addr_str(pc+2 + (int8_t)*data));
                data++;
                break;
            }
            break;
        case 1:
            if (q) {
                snprintf(buf, buflen, "add hl, %s", t_rp[p]);
            } else {
                snprintf(buf, buflen, "ld %s, %s", t_rp[p], get_paddr_str(data));
                data += 2;
            }
            break;
        case 2:
            if (y < 4) {
                strncpy(buf, t_x0_z2[y], buflen);
            } else {
                snprintf(buf, buflen, t_x0_z2[y], get_paddr_str(data));
                data += 2;
            }
            break;
        case 3:
            if (q) {
                snprintf(buf, buflen, "dec %s", t_rp[p]);
            } else {
                snprintf(buf, buflen, "inc %s", t_rp[p]);
            }
            break;
        case 4:
            snprintf(buf, buflen, "inc %s", t_r[y]);
            break;
        case 5:
            snprintf(buf, buflen, "dec %s", t_r[y]);
            break;
        case 6:
            snprintf(buf, buflen, "ld %s, %s", t_r[y], get_byte_str(*data));
            data++;
            break;
        case 7:
            strncpy(buf, t_x0_z7[y], buflen);
            break;
        }
        break;
    case 1:
        if (z == 6 && y == 6)
            strncpy(buf, "halt", buflen);
        else
            snprintf(buf, buflen, "ld %s, %s", t_r[y], t_r[z]);
        break;
    case 2:
        snprintf(buf, buflen, "%s %s", t_alu[y], t_r[z]);
        break;
    case 3:
        switch (z)
        {
        case 0:
            snprintf(buf, buflen, "ret %s", t_cc[y]);
            break;
        case 1:
            if (q)
                strncpy(buf, t_x1_z1_q[p], buflen);
            else
                snprintf(buf, buflen, "pop %s", t_rp2[p]);
            break;
        case 2:
            snprintf(buf, buflen, "jp %s, %s", t_cc[y], get_paddr_str(data));
            data += 2;
            break;
        case 3:
            switch (y)
            {
            case 0:
                snprintf(buf, buflen, "jp %s", get_paddr_str(data));
                data += 2;
                break;
            case 1:
                data += prefix_cb(data, buf, buflen);
                break;
            case 2:
                snprintf(buf, buflen, "out (%s), a", get_byte_str(*data));
                data++;
                break;
            case 3:
                snprintf(buf, buflen, "in a, (%s)", get_byte_str(*data));
                data++;
                break;
            default:
                strncpy(buf, t_x3_z3[y-4], buflen);
            }
            break;
        case 4:
            snprintf(buf, buflen, "call %s, %s", t_cc[y], get_paddr_str(data));
            data += 2;
            break;
        case 5:
            if (q) {
                switch (p)
                {
                case 0:
                    snprintf(buf, buflen, "call %s", get_byte_str(*data));
                    data += 2;
                case 1:
                    // dd
                    break;
                case 2:
                    data += prefix_ed(data, buf, buflen);
                    break;
                case 3:
                    // fd
                    break;
                }
            } else {
                snprintf(buf, buflen, "push %s", t_rp2[p]);
            }
            break;
        case 6:
            snprintf(buf, buflen, "%s %s", t_alu[y], get_byte_str(*data));
            data++;
            break;
        case 7:
            snprintf(buf, buflen, "rst %s", get_byte_str(y*8));
            break;
        }
    }

    *len = data - dorg;

    size_t slen = strlen(buf) + 1;
    char *str = malloc(slen);
    if (str == NULL) {
        return NULL;
    }
    memcpy(str, buf, slen-1);
    str[slen-1] = 0;

    return str;
}
