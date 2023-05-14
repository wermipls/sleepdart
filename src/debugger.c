#include "debugger.h"

#include "microui.h"
#include "microui_render.h"
#include "machine.h"
#include "z80.h"
#include "disasm.h"
#include "vector.h"
#include <SDL2/SDL_video.h>
#include <SDL2/SDL_events.h>

static mu_Context context;
static Machine_t *machine;

const char *icons[] = {
    "",
    "()",
    "->",
};

struct Disasm
{
    int icon;
    uint16_t pcval;
    char *pc;
    char *bytes;
    char *instr;
};

struct Breakpoint
{
    int enabled;
    uint16_t pc;
};

static struct Breakpoint breakpoints[256];
static char registers[1024];

struct Disasm *disasm = NULL;

int mu_button_ex_id(mu_Context *ctx, const char *label, int id_num, int icon, int opt) {
    int res = 0;
    mu_Id id = id_num ? mu_get_id(ctx, &id_num, sizeof(id_num))
                      : mu_get_id(ctx, &icon, sizeof(icon));
    mu_Rect r = mu_layout_next(ctx);
    mu_update_control(ctx, id, r, opt);
    /* handle click */
    if (ctx->mouse_pressed == MU_MOUSE_LEFT && ctx->focus == id) {
        res |= MU_RES_SUBMIT;
    }
    /* draw */
    mu_draw_control_frame(ctx, id, r, MU_COLOR_BUTTON, opt);
    if (label) { mu_draw_control_text(ctx, label, r, MU_COLOR_TEXT, opt); }
    if (icon) { mu_draw_icon(ctx, icon, r, ctx->style->colors[MU_COLOR_TEXT]); }
    return res;
}

static int is_breakpoint(uint16_t pc)
{
    for (int i = 0; i < 256; i++) {
        if (breakpoints[i].enabled && pc == breakpoints[i].pc) {
            return 1;
        }
    }
    return 0;
}

static void add_breakpoint(uint16_t pc)
{
    for (int i = 0; i < 256; i++) {
        if (!breakpoints[i].enabled) {
            breakpoints[i].enabled = 1;
            breakpoints[i].pc = pc;
            return;
        }
    }
}

static void remove_breakpoint(uint16_t pc)
{
    for (int i = 0; i < 256; i++) {
        if (breakpoints[i].enabled && pc == breakpoints[i].pc) {
            breakpoints[i].enabled = 0;
        }
    }
}

static void update_regs()
{
    struct Z80Regs *r = &machine->cpu.regs;
    snprintf(registers, sizeof(registers),
        "PC  %04x    SP  %04x\n\n"
        "AF  %04x    AF` %04x\n"
        "BC  %04x    BC` %04x\n"
        "DE  %04x    DE` %04x\n"
        "HL  %04x    HL` %04x\n"
        "IX  %04x    IY  %04x\n",
        r->pc,      r->sp,
        r->main.af, r->alt.af,
        r->main.bc, r->alt.bc,
        r->main.de, r->alt.de,
        r->main.hl, r->alt.hl,
        r->ix,      r->iy);
}

int dbg_continue = 0;

static void disasm_window(mu_Context *ctx) {
    if (mu_begin_window_ex(ctx, "Debugger", mu_rect(0, 0, 400, 600), MU_OPT_NOCLOSE)) {
        int pc_changed = 0;
        
        mu_layout_row(ctx, 3, (int[]) { -200, -100, -1 }, 25);
        if (mu_button(ctx, "Continue") || dbg_continue) {
            dbg_continue = 1;
            int timeout = 70000; // hack
            uint16_t pc = machine->cpu.regs.pc;
            do {
                cpu_do_cycles(&machine->cpu);
                if (is_breakpoint(machine->cpu.regs.pc)) {
                    dbg_continue = 0;
                    break;
                }
            } while (timeout--);
            update_regs();
            pc_changed = 1;
        };
        mu_button(ctx, "Step over");
        if (mu_button(ctx, "Step into")) {
            uint16_t pc = machine->cpu.regs.pc;
            cpu_do_cycles(&machine->cpu);
            update_regs();
            pc_changed = (machine->cpu.regs.pc != pc);
        };
        
        mu_layout_row(ctx, 1, (int[]) { -1 }, -1);
        mu_begin_panel(ctx, "disassembly");
        mu_Container *panel = mu_get_current_container(ctx);
        int line_h = render_text_height(0) + 4;
        int pci = 0;
        for (size_t i = 0; i < vector_len(disasm); i++) {
            struct Disasm *d = &disasm[i];
            int icon = d->icon;
            if (d->pcval == machine->cpu.regs.pc) {
                icon = 2;
                pci = i;
            }
            mu_layout_row(ctx, 4, (int[]) { 16, 32, 100, -1 }, line_h);
            if (mu_button_ex_id(ctx, icons[icon], 1+i, 0, MU_OPT_NOFRAME)) {
                d->icon ^= 1;
                if (d->icon) add_breakpoint(d->pcval);
                else remove_breakpoint(d->pcval);
            }
            mu_label(ctx, d->pc);
            mu_label(ctx, d->bytes);
            mu_label(ctx, d->instr);
        }
        if (pc_changed) {
            int curline_y = (line_h + ctx->style->spacing) * pci;
            int offset = curline_y - panel->scroll.y;
            if (offset < 100 || offset > (panel->body.h - 100)) {
                panel->scroll.y = curline_y - panel->body.h / 2;
                if (panel->scroll.y < 0) panel->scroll.y = 0;
            }
        }
        mu_end_panel(ctx);

        mu_end_window(ctx);
    }
}

static void regs_window(mu_Context *ctx)
{
    if (mu_begin_window(ctx, "Registers", mu_rect(400, 0, 200, 200))) {
        mu_layout_row(ctx, 1, (int[]) { -1 }, -1);
        mu_text(ctx, registers);
        mu_end_window(ctx);
    }
}

static void process_frame(mu_Context *ctx) {
    mu_begin(ctx);
    disasm_window(ctx);
    regs_window(ctx);
    mu_end(ctx);
}

static int window_loop(mu_Context *ctx)
{
    SDL_Event e;
    SDL_PumpEvents();
    while (SDL_PollEvent(&e)) {
        switch (e.type)
        {
        case SDL_MOUSEMOTION:
            mu_input_mousemove(ctx, e.motion.x, e.motion.y);
            break;
        case SDL_MOUSEBUTTONDOWN:
            mu_input_mousedown(ctx, e.button.x, e.button.y, e.button.button);
            break;
        case SDL_MOUSEBUTTONUP:
            mu_input_mouseup(ctx, e.button.x, e.button.y, e.button.button);
            break;
        case SDL_WINDOWEVENT:
            switch (e.window.event)
            {
            case SDL_WINDOWEVENT_CLOSE: return 0; break;
            } 
        }
    }

    process_frame(ctx);
    process_frame(ctx);

    render_clear(mu_color(0, 0, 0, 255));
    mu_Command *cmd = NULL;
    while (mu_next_command(ctx, &cmd)) {
        switch(cmd->type)
        {
        case MU_COMMAND_TEXT: render_text(cmd->text.font, cmd->text.str, cmd->text.pos, cmd->text.color); break;
        case MU_COMMAND_RECT: render_draw_rect(cmd->rect.rect, cmd->rect.color); break;
        }
    }
    render_present();

    return 1;
}


void debugger_open(Machine_t *m)
{
    mu_init(&context);
    context.text_height = render_text_height;
    context.text_width = render_text_width;
    
    if (render_init()) {
        return;
    }

    disasm = vector_create();

    memset(breakpoints, 0, sizeof(breakpoints));

    char buf[1024];

    machine = m;

    uint8_t *pmem = machine->memory.bus;
    uint16_t pc = 0;
    for (; pc < 0x4000;) {
        struct Disasm d = { 0 };
        d.pcval = pc;

        int index = 0;
        int len = 1;
        char *op = disasm_opcode(pmem, &len, pc);

        snprintf(
            buf, sizeof(buf),
            "%04x", pc);
        size_t size = strlen(buf)+1;
        d.pc = malloc(size);
        if (d.pc == NULL) {
            free(op);
            break;
        }
        memcpy(d.pc, buf, size-1);
        d.pc[size-1] = 0;

        while (len--) {
            index += snprintf(
                buf+index, sizeof(buf)-index,
                "%02x ", *pmem);
            pmem++;
            pc++;
        }

        size = strlen(buf)+1;
        d.bytes = malloc(size);
        if (d.bytes == NULL) {
            free(d.pc);
            free(op);
            break;
        }
        memcpy(d.bytes, buf, size-1);
        d.bytes[size-1] = 0;

        d.instr = op;
        vector_add(disasm, d);
    }

    update_regs();

    while (window_loop(&context));

    render_deinit();
    vector_free(disasm);
}

