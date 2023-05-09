#include "machine_hooks.h"
#include <stdio.h>
#include "machine.h"
#include "video_sdl.h"

static void putc_zx(uint8_t ch, FILE *f)
{
    char *s = NULL;
    switch (ch)
    {
    case 0x0D: s = "\n"; break;
    case 0x5E: s = "↑"; break;
    case 0x60: s = "£"; break;
    case 0x7F: s = "©"; break;
    case 0x80: s = " "; break;
    case 0x81: s = "▝"; break;
    case 0x82: s = "▘"; break;
    case 0x83: s = "▀"; break;
    case 0x84: s = "▗"; break;
    case 0x85: s = "▐"; break;
    case 0x86: s = "▚"; break;
    case 0x87: s = "▜"; break;
    case 0x88: s = "▖"; break;
    case 0x89: s = "▞"; break;
    case 0x8A: s = "▌"; break;
    case 0x8B: s = "▛"; break;
    case 0x8C: s = "▄"; break;
    case 0x8D: s = "▟"; break;
    case 0x8E: s = "▙"; break;
    case 0x8F: s = "█"; break;
    
    default:
        if (ch < 0x20 || ch >= 0x80) return;
        putc(ch, f);
    }

    fputs(s, f);
}

void machine_process_hooks(struct Machine *m)
{
    static bool inside_tape_routine = false;

    uint16_t pc = m->cpu.regs.pc;

    if (pc == 0x0556) {
        inside_tape_routine = true;
        video_sdl_set_fps_limit(false);
        tape_player_pause(m->player, false);
    }

    if (inside_tape_routine && (pc < 0x0556 || pc >= 0x0605)) {
        inside_tape_routine = false;
        video_sdl_set_fps_limit(true);
        tape_player_pause(m->player, true);
    }

    if (pc == 0x09F4) {
        uint8_t tv_flag, flags2;
        memory_read(m, m->cpu.regs.iy + 0x02, &tv_flag);
        memory_read(m, m->cpu.regs.iy + 0x30, &flags2);
        if (!(flags2 & (1<<4)) && !(tv_flag & 1)) {
            putc_zx(m->cpu.regs.main.a, stdout);
        }
    }
}
