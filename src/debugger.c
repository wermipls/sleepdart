#include "debugger.h"

#include "microui.h"
#include "microui_render.h"
#include "machine.h"
#include "z80.h"
#include "disasm.h"
#include "vector.h"
#include "input_sdl.h"
#include <SDL2/SDL_video.h>
#include <SDL2/SDL_events.h>

static mu_Context context;
static Machine_t *machine;

struct Disasm
{
    int breakpoint;
    uint16_t pc;
    int len;
    int dirty;
    char *pcstr;
    char *bytestr;
    char *instr;
};

struct Breakpoint
{
    int enabled;
    uint16_t pc;
};

enum Condition
{
    DBG_NONE,
    DBG_STEPINTO,
    DBG_STEPOVER,
    DBG_BREAKPOINT,
};

static struct Breakpoint breakpoints[256];
static char registers[1024];
static enum Condition break_on = DBG_STEPINTO;
static bool debugger_enabled = false;

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

static int disasm_update(struct Disasm *d, uint16_t pc)
{
    *d = (struct Disasm) { 0 };
    d->pc = pc;

    char buf[256];

    int index = 0;
    int len = 1;
    char *op = disasm_opcode(&machine->memory.bus[pc], &len, pc);
    d->len = len;

    snprintf(
        buf, sizeof(buf),
        "%04x", pc);
    size_t size = strlen(buf)+1;
    d->pcstr = malloc(size);
    if (d->pcstr == NULL) {
        free(op);
        return -1;
    }
    memcpy(d->pcstr, buf, size-1);
    d->pcstr[size-1] = 0;

    uint16_t i = pc;
    while (len--) {
        index += snprintf(
            buf+index, sizeof(buf)-index,
            "%02x ", machine->memory.bus[i]);
        i++;
    }

    size = strlen(buf)+1;
    d->bytestr = malloc(size);
    if (d->bytestr == NULL) {
        free(d->pcstr);
        free(op);
        return -2;
    }
    memcpy(d->bytestr, buf, size-1);
    d->bytestr[size-1] = 0;

    d->instr = op;
    return 0;
}

static void disasm_free(struct Disasm *d)
{
    if (d->bytestr) { free(d->bytestr); d->bytestr = NULL; }
    if (d->instr) { free(d->instr); d->instr = NULL; }
    if (d->pcstr) { free(d->pcstr); d->pcstr = NULL; }
}

void debugger_mark_dirty(uint16_t addr)
{
    if (!debugger_enabled) return;

    disasm[addr--].dirty = true;
    disasm[addr--].dirty = true;
    disasm[addr--].dirty = true;
    disasm[addr--].dirty = true;
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
        "IX  %04x    IY  %04x\n\n"
        "cycles: %05lld/%05u",
        r->pc,      r->sp,
        r->main.af, r->alt.af,
        r->main.bc, r->alt.bc,
        r->main.de, r->alt.de,
        r->main.hl, r->alt.hl,
        r->ix,      r->iy,
        machine->cpu.cycles, machine->timing.t_frame);
}

static void disasm_window(mu_Context *ctx) {
    if (mu_begin_window_ex(ctx, "Debugger", mu_rect(0, 0, 400, 600), MU_OPT_NOCLOSE)) {
        static int pc = -1;
        int pc_changed = (machine->cpu.regs.pc != pc);
        pc = machine->cpu.regs.pc;

        mu_layout_row(ctx, 4, (int[]) { 70, -200, -100, -1 }, 25);
        if (mu_button(ctx, "Pause") && break_on == DBG_BREAKPOINT) {
            break_on = DBG_STEPINTO;
        };
        if (mu_button(ctx, "Continue")) {
            break_on = DBG_BREAKPOINT;
        };
        mu_button(ctx, "Step over");
        if (mu_button(ctx, "Step into")) {
             break_on = DBG_STEPINTO;
        };
        
        mu_layout_row(ctx, 1, (int[]) { -1 }, -1);
        mu_begin_panel(ctx, "disassembly");
        mu_Container *panel = mu_get_current_container(ctx);
        int line_h = render_text_height(0) + 4;
        int disasm_lines = 0;
        int pclines = 0;
        size_t i = machine->cpu.regs.pc;
        i = i > 32 ? i - 32 : 0;
        for (; i < vector_len(disasm) && disasm_lines < 64;) {
            struct Disasm *d = &disasm[i];
            if (d->dirty) {
                disasm_free(d);
                disasm_update(d, i);
            }
            mu_layout_row(ctx, 4, (int[]) { line_h, 32, 100, -1 }, line_h);
            int icon = d->breakpoint ? DBGICON_BREAKPOINT : 0;
            if (d->pc == machine->cpu.regs.pc) {
                icon = DBGICON_CURRENT;
                pclines = disasm_lines;
            }
            if (mu_button_ex_id(ctx, NULL, 1+i, icon, MU_OPT_NOFRAME)) {
                d->breakpoint ^= 1;
                if (d->breakpoint) add_breakpoint(d->pc);
                else remove_breakpoint(d->pc);
            }
            mu_label(ctx, d->pcstr);
            mu_label(ctx, d->bytestr);
            mu_label(ctx, d->instr);

            disasm_lines++;
            i += d->len;
        }
        if (pc_changed) {
            int curline_y = (line_h + ctx->style->spacing) * pclines;
            panel->scroll.y = curline_y - panel->body.h / 2;
            if (panel->scroll.y < 0) panel->scroll.y = 0;
        }
        mu_end_panel(ctx);

        mu_end_window(ctx);
    }
}

static void regs_window(mu_Context *ctx)
{
    if (mu_begin_window(ctx, "Registers", mu_rect(400, 0, 200, 400))) {
        mu_layout_row(ctx, 1, (int[]) { -1 }, 220);
        mu_text(ctx, registers);

        mu_layout_row(ctx, 8, (int[]) { 16, 16, 16, 16, 16, 16, 16, 16 }, 16);
        static const char *flag_label[8] = {
            "C", "N", "PV", "X", "H", "Y", "Z", "S"
        };
        for (int i = 7; i >= 0; i--) {
            mu_draw_control_text(
                ctx, flag_label[i], mu_layout_next(ctx), 
                MU_COLOR_TEXT, MU_OPT_ALIGNCENTER);
        }
        int flags[8];
        int clicked = 0;
        for (int i = 7; i >= 0; i--) {
            flags[i] = machine->cpu.regs.main.f & (1<<i);
            clicked |= mu_checkbox(ctx, "", &flags[i]);
        }
        if (clicked) {
            uint8_t f = 0;
            for (int i = 0; i < 8; i++) {
                f |= (1<<i) * !(!flags[i]);
            }
            machine->cpu.regs.main.f = f;
            update_regs();
        }
        mu_end_window(ctx);
    }
}

static void process_frame(mu_Context *ctx) {
    mu_begin(ctx);
    disasm_window(ctx);
    regs_window(ctx);
    mu_end(ctx);
}

static int window_loop(mu_Context *ctx, int flush_events)
{
    SDL_Event e;
    SDL_Event events[1024];
    if (flush_events) SDL_PumpEvents();
    int count = SDL_PeepEvents(events, 1024, SDL_PEEKEVENT, SDL_FIRSTEVENT, SDL_LASTEVENT);

    for (int i = 0; i < count; i++) {
        e = events[i];
        switch (e.type)
        {
        case SDL_MOUSEMOTION:
            if (e.motion.windowID != render_get_window_id()) break;
            mu_input_mousemove(ctx, e.motion.x, e.motion.y);
            break;
        case SDL_MOUSEBUTTONDOWN:
            if (e.button.windowID != render_get_window_id()) break;
            mu_input_mousedown(ctx, e.button.x, e.button.y, e.button.button);
            break;
        case SDL_MOUSEBUTTONUP:
            if (e.button.windowID != render_get_window_id()) break;
            mu_input_mouseup(ctx, e.button.x, e.button.y, e.button.button);
            break;
        case SDL_WINDOWEVENT:
            if (e.window.windowID != render_get_window_id()) break;
            if (e.window.event == SDL_WINDOWEVENT_CLOSE) {
                debugger_close();
                break_on = DBG_NONE;
                return 0;
            }
            break;
        case SDL_MOUSEWHEEL:
            if (e.wheel.windowID != render_get_window_id()) break;
            mu_input_scroll(ctx, 0, e.wheel.y * -30);
            break;
        }
    }

    if (flush_events) {
        input_sdl_update();
        SDL_FlushEvents(SDL_FIRSTEVENT, SDL_LASTEVENT);
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
        case MU_COMMAND_CLIP: render_clip_rect(cmd->rect.rect); break;
        case MU_COMMAND_ICON: render_draw_icon(cmd->icon.id, cmd->icon.rect, cmd->icon.color); break;
        }
    }
    render_present();

    if (break_on != DBG_NONE) {
        return 0;
    }

    return 1;
}

void debugger_handle()
{
    if (!debugger_enabled) return;

    bool is_break = false;

    switch (break_on)
    {
    case DBG_BREAKPOINT:
        if (is_breakpoint(machine->cpu.regs.pc)) {
            is_break = true;
        }
        break;
    case DBG_STEPOVER:
    case DBG_STEPINTO:
        is_break = true;
        break;
    default:
        break;
    }

    if (is_break) {
        break_on = DBG_NONE;
        update_regs();
        while (window_loop(&context, true));
    }
}

void debugger_update_window()
{
    if (!debugger_enabled) return;

    update_regs();
    window_loop(&context, false);
}

void debugger_open(Machine_t *m)
{
    if (debugger_enabled) return;

    mu_init(&context);
    context.text_height = render_text_height;
    context.text_width = render_text_width;
    
    if (render_init()) {
        return;
    }

    disasm = vector_create();

    memset(breakpoints, 0, sizeof(breakpoints));

    machine = m;

    int pc = 0;
    for (; pc < 0x10000;) {
        struct Disasm d = { 0 };
        disasm_update(&d, pc);
        vector_add(disasm, d);

        pc++;
    }

    update_regs();

    debugger_enabled = true;
}

void debugger_close()
{
    if (!debugger_enabled) return;

    render_deinit();
    vector_free(disasm);

    debugger_enabled = false;
}

