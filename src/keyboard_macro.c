#include "keyboard_macro.h"
#include "file.h"
#include "log.h"
#include "parser_helpers.h"
#define CC_NO_SHORT_NAMES
#include "../external/cc.h"

enum KeyMacroCmd
{
    KBMACRO_KEY,
    KBMACRO_GOTO,
};

struct KeyboardMacroOp {
    int frame;
    enum KeyMacroCmd cmd;
    int value;
};

struct KeyboardMacro {
    const struct KeyboardMacroOp *ops;
    size_t len;
    cc_vec(struct KeyboardMacroOp) vec;
};

struct MacroState {
    const KeyboardMacro_t *macro;
    int frame;
    int index;
    uint8_t state[8];
};

enum MacroType {
    MACRO_NORMAL = 0,
    MACRO_TAPE,
    MACRO_ENUM_LAST,
};

static struct MacroState macro_state[MACRO_ENUM_LAST] = {0};

const struct KeyboardMacroOp macro_tapeload_ops[6] = {
    { 10, KBMACRO_KEY, 33 }, // LOAD
    { 15, KBMACRO_KEY, 36 }, // SYM SHFT
    { 15, KBMACRO_KEY, 25 }, // "
    { 20, KBMACRO_KEY, 36 }, // SYM SHFT
    { 20, KBMACRO_KEY, 25 }, // "
    { 25, KBMACRO_KEY, 30 }, // ENTER
};

const KeyboardMacro_t macro_tapeload = {
    .ops = macro_tapeload_ops,
    .len = sizeof(macro_tapeload_ops) / sizeof(*macro_tapeload_ops),
};

void keyboard_macro_init()
{
    for (int i = 0; i < MACRO_ENUM_LAST; i++) {
        struct MacroState *p = &macro_state[i];
        p->macro = 0;
        for (int i = 0; i < 8; i++) {
            p->state[i] = 0xFF;
        }
    }
}

static void keyboard_macro_play_internal(struct MacroState *p, const KeyboardMacro_t *macro)
{
    p->macro = macro;
    p->frame = 0;
    p->index = 0;
    for (int i = 0; i < 8; i++) {
        p->state[i] = 0xFF;
    }
}

void keyboard_macro_play(const KeyboardMacro_t *macro)
{
    keyboard_macro_play_internal(&macro_state[MACRO_NORMAL], macro);
}

void keyboard_macro_play_tapeload()
{
    keyboard_macro_play_internal(
        &macro_state[MACRO_TAPE],
        &macro_tapeload
    );
}

static inline void keyboard_macro_process_single(struct MacroState *p)
{
    if (p->macro == NULL) {
        return;
    }

    for (int i = 0; i < 8; i++) {
        p->state[i] = 0xFF;
    }

    if (p->index >= p->macro->len) {
        p->macro = NULL;
        return;
    }

    p->frame++;

    while (p->macro->ops[p->index].frame < p->frame) {
        p->index++;
        if (p->index >= p->macro->len) {
            return;
        }
    }

    while (p->macro->ops[p->index].frame == p->frame) {
        switch (p->macro->ops[p->index].cmd)
        {
        case KBMACRO_GOTO:
            p->frame = p->macro->ops[p->index].value - 1;
            p->index = 0;
            return;
        case KBMACRO_KEY: ;
            int addr = (p->macro->ops[p->index].value / 5) & 7;
            int bit = p->macro->ops[p->index].value % 5;
            p->state[addr] &= ~(1<<bit);
            break;
        }

        p->index++;
        if (p->index >= p->macro->len) {
            return;
        }
    }
}

void keyboard_macro_process()
{
    for (int i = 0; i < MACRO_ENUM_LAST; i++) {
        struct MacroState *p = &macro_state[i];
        keyboard_macro_process_single(p);
    }
}

uint8_t keyboard_macro_get(int address)
{
    return macro_state[MACRO_NORMAL].state[address] & macro_state[MACRO_TAPE].state[address];
}

KeyboardMacro_t *keyboard_macro_parse(const char *path)
{
    FILE *f = fopen_utf8(path, "r");
    if (f == NULL) {
        return NULL;
    }

    cc_vec(struct KeyboardMacroOp) ops;
    cc_init(&ops);

    int line_no = 0;
    char *line;
    while ((line = file_read_line(f)) != NULL) {
        line_no++;
        char *last;
        char *pframe = strtok_r(line, " ", &last);
        if (pframe == NULL) {
            goto cleanup;
        }

        char *pcmd = strtok_r(NULL, " ", &last);
        if (pcmd == NULL) {
            goto cleanup;
        }

        char *pvalue = strtok_r(NULL, " ", &last);
        if (pvalue == NULL) {
            goto cleanup;
        }

        struct KeyboardMacroOp m;

        if (strcmp("key", pcmd) == 0) {
            m.cmd = KBMACRO_KEY;
        } else if (strcmp("goto", pcmd) == 0) {
            m.cmd = KBMACRO_GOTO;
        } else {
            dlog(LOG_WARN, "Unknown macro cmd on line %d", line_no);
            goto cleanup;
        }

        if (!parse_int(pframe, &m.frame)) {
            dlog(LOG_WARN, "Failed to parse macro frame on line %d", line_no);
            goto cleanup;
        }

        if (!parse_int(pvalue, &m.value)) {
            dlog(LOG_WARN, "Failed to parse macro value on line %d", line_no);
            goto cleanup;
        }

        cc_push(&ops, m);
    cleanup:
        free(line);
    }

    fclose(f);

    if (cc_size(&ops) == 0) {
        cc_cleanup(&ops);
        return NULL;
    }

    KeyboardMacro_t *macro = malloc(sizeof(KeyboardMacro_t));
    if (!macro) {
        cc_cleanup(&ops);
        return NULL;
    }

    macro->ops = cc_first(&ops);
    macro->len = cc_size(&ops);
    macro->vec = ops;

    return macro;
}

void keyboard_macro_free(KeyboardMacro_t *macro)
{
    if (!macro) return;

    cc_cleanup(&macro->vec);
    free(macro);
}
