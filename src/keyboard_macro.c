#include "keyboard_macro.h"
#include "file.h"
#include "vector.h"
#include "log.h"
#include "parser_helpers.h"

struct MacroState {
    const KeyboardMacro_t *current;
    int frame;
    int index;
    int macro_len;
    uint8_t state[8];
};

enum MacroType {
    MACRO_NORMAL = 0,
    MACRO_TAPE,
    MACRO_ENUM_LAST,
};

static struct MacroState macro_state[MACRO_ENUM_LAST] = {0};

const KeyboardMacro_t macro_tapeload[6] = {
    { 10, KBMACRO_KEY, 33 }, // LOAD
    { 15, KBMACRO_KEY, 36 }, // SYM SHFT
    { 15, KBMACRO_KEY, 25 }, // "
    { 20, KBMACRO_KEY, 36 }, // SYM SHFT
    { 20, KBMACRO_KEY, 25 }, // "
    { 25, KBMACRO_KEY, 30 }, // ENTER
};

void keyboard_macro_init()
{
    for (int i = 0; i < MACRO_ENUM_LAST; i++) {
        struct MacroState *p = &macro_state[i];
        p->current = 0;
        for (int i = 0; i < 8; i++) {
            p->state[i] = 0xFF;
        }
    }
}

static void keyboard_macro_play_internal(struct MacroState *p, const KeyboardMacro_t *macro, size_t len)
{
    p->current = macro;
    p->frame = 0;
    p->index = 0;
    p->macro_len = len;
    for (int i = 0; i < 8; i++) {
        p->state[i] = 0xFF;
    }
}

void keyboard_macro_play(const KeyboardMacro_t *macro, size_t len)
{
    keyboard_macro_play_internal(&macro_state[MACRO_NORMAL], macro, len);
}

void keyboard_macro_play_tapeload()
{
    keyboard_macro_play_internal(
        &macro_state[MACRO_TAPE],
        macro_tapeload,
        sizeof(macro_tapeload) / sizeof(KeyboardMacro_t)
    );
}

static inline void keyboard_macro_process_single(struct MacroState *p)
{
    if (p->current == NULL) {
        return;
    }

    for (int i = 0; i < 8; i++) {
        p->state[i] = 0xFF;
    }

    if (p->index >= p->macro_len) {
        p->current = NULL;
        return;
    }

    p->frame++;

    while (p->current[p->index].frame < p->frame) {
        p->index++;
        if (p->index >= p->macro_len) {
            return;
        }
    }

    while (p->current[p->index].frame == p->frame) {
        switch (p->current[p->index].cmd)
        {
        case KBMACRO_GOTO:
            p->frame = p->current[p->index].value - 1;
            p->index = 0;
            return;
        case KBMACRO_KEY: ;
            int addr = (p->current[p->index].value / 5) & 7;
            int bit = p->current[p->index].value % 5;
            p->state[addr] &= ~(1<<bit);
            break;
        }

        p->index++;
        if (p->index >= p->macro_len) {
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

    KeyboardMacro_t *macro = vector_create();
    if (macro == NULL) {
        fclose(f);
        return NULL;
    }

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

        KeyboardMacro_t m;

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

        vector_add(macro, m);
    cleanup:
        free(line);
    }

    fclose(f);

    if (vector_len(macro) == 0) {
        vector_free(macro);
        return NULL;
    }

    return macro;
}
