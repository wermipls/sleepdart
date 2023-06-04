#include "machine_test.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "machine.h"
#include "machine_hooks.h"
#include "file.h"
#include "config_parser.h"
#include "log.h"
#include "keyboard_macro.h"
#include "parser_helpers.h"
#include "vector.h"
#include "video_sdl.h"

#define XXH_INLINE_ALL
#include <xxhash.h>

enum StopCondition
{
    STOP_BREAKPOINT,
    STOP_FRAME,
};

const char *condition_str[] = {
    "breakpoint",
    "frame",
};

struct MachineTest
{
    char *dir;
    enum StopCondition stop_condition;
    int stop_value;
    bool test_docflags;
    bool test_allflags;
    bool test_registers;
    bool test_cycles;
    bool test_print;
    XXH64_state_t *docflags;
    XXH64_state_t *allflags;
    XXH64_state_t *registers;
    XXH64_state_t *cycles;
    FILE *print;
    KeyboardMacro_t *macro;
};

static struct CfgField test_fields[] = {
    { "file",           CFG_STR, NULL },
    { "stop-condition", CFG_STR, NULL },
    { "stop-value",     CFG_INT, NULL },
    { "scope",          CFG_STR, NULL },
    { "macro",          CFG_STR, NULL },
};

static CfgData_t testcfg = {
    .data = test_fields,
    .len = sizeof(test_fields) / sizeof(struct CfgField)
};

static bool test_running = false;
static bool test_passed = true;
static struct MachineTest test = { 0 };

static KeyboardMacro_t *parse_macro(const char *path)
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

        int *frame = parse_int(pframe);
        if (frame == NULL) {
            dlog(LOG_WARN, "Failed to parse macro frame on line %d", line_no);
            goto cleanup;
        }
        m.frame = *frame;
        free(frame);

        int *value = parse_int(pvalue);
        if (value == NULL) {
            dlog(LOG_WARN, "Failed to parse macro value on line %d", line_no);
            goto cleanup;
        }
        m.value = *value;
        free(value);

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

int machine_test_open(const char *path)
{
    if (path == NULL) {
        return -1;
    }

    machine_test_close();

    char buf[2048];
    file_path_append(buf, path, "sleepdart-test.ini", sizeof(buf));
    int err = config_load_file(&testcfg, buf);
    if (err) {
        dlog(LOG_ERRSILENT, "Failed to open test file \"%s\"", buf);
        return -2;
    }

    size_t dir_len = strlen(path) + 1;
    char *dir = malloc(dir_len);
    if (dir == NULL) {
        dlog(LOG_ERRSILENT, "%s: malloc fail", __func__);
        return -3;
    }
    strncpy(dir, path, dir_len-1);
    dir[dir_len-1] = 0;
    test.dir = dir;

    char *file = config_get_str(&testcfg, "file");
    if (file) {
        file_path_append(buf, path, file, sizeof(buf));
        machine_open_file(buf);
        free(file);
    }

    char *condition = config_get_str(&testcfg, "stop-condition");

    if (condition == NULL) {
        dlog(LOG_ERRSILENT, "Missing stop-condition parameter in \"%s\"", buf);
        return -4;
    }

    bool match = 0;
    for (size_t i = 0; i < sizeof(condition_str) / sizeof(char *); i++) {
        if (strcmp(condition, condition_str[i]) == 0) {
            test.stop_condition = i;
            match = true;
        }
    }

    if (!match) {
        dlog(LOG_ERRSILENT, "Unrecognized stop-condition value \"%s\"", condition);
        free(condition);
        return -5;
    }
    free(condition);

    if (config_get_int(&testcfg, "stop-value", &test.stop_value) != 0) {
        dlog(LOG_ERRSILENT, "Missing stop-value parameter");
        return -6;
    }

    char *scope = config_get_str(&testcfg, "scope");
    if (scope == NULL) {
        test.test_docflags = true;
        test.test_registers = true;
        test.test_cycles = true;
    } else {
        char *last;
        char *token;
        char *str = scope;
        while ((token = strtok_r(str, " ", &last)) != NULL) {
            if (strcmp("docflags", token) == 0) {
                test.test_docflags = true;
            } else if (strcmp("allflags", token) == 0) {
                test.test_allflags = true;
            } else if (strcmp("registers", token) == 0) {
                test.test_registers = true;
            } else if (strcmp("cycles", token) == 0) {
                test.test_cycles = true;
            } else if (strcmp("print", token) == 0) {
                test.test_print = true;
            } else {
                dlog(LOG_WARN, "Unknown test scope \"%s\"", token);
            }
            str = NULL;
        }
        free(scope);
    }

    if (test.test_allflags) {
        test.allflags = XXH64_createState();
        XXH64_reset(test.allflags, 0);
    }
    if (test.test_docflags) {
        test.docflags  = XXH64_createState();
        XXH64_reset(test.docflags, 0);
    }
    if (test.test_registers) { 
        test.registers = XXH64_createState();
        XXH64_reset(test.registers, 0);
    }
    if (test.test_cycles) {
        test.cycles = XXH64_createState();
        XXH64_reset(test.cycles, 0);
    }
    if (test.test_print) {
        file_path_append(buf, path, "print.txt.tmp", sizeof(buf));
        test.print = fopen_utf8(buf, "wb+");
        if (test.print == NULL) {
            dlog(LOG_ERR, "Failed to open file \"%s\" for write", buf);
            return -8;
        }
        machine_set_print_stream(test.print);
    }

    char *macro = config_get_str(&testcfg, "macro");
    if (macro) {
        file_path_append(buf, path, macro, sizeof(buf));
        free(macro);
        test.macro = parse_macro(buf);
        if (test.macro == NULL) {
            dlog(LOG_WARN, "Failed to parse macro file \"%s\"", buf);
            return -8;
        } else {
            keyboard_macro_play(test.macro, vector_len(test.macro));
        }
    }

    test_running = true;
    video_sdl_set_fps_limit(false);

    return 0;
}

static int test_condition(struct Machine *m)
{
    switch (test.stop_condition) {
    case STOP_BREAKPOINT:
        if (m->cpu.regs.pc == test.stop_value) return 1;
        break;
    case STOP_FRAME:
        if (m->frames == test.stop_value) return 1;
        break;
    }

    return 0;
}

static void finish_hash(XXH64_state_t *s, const char *name)
{
    if (s == NULL) return;

    XXH64_hash_t hash = XXH64_digest(s);
    dlog(LOG_INFO, "%s hash: %016llx", name, hash);
    uint64_t expected;
    char buf[2048];
    file_path_append(buf, test.dir, name, sizeof(buf));
    FILE *f = fopen_utf8(buf, "rb");
    if (f == NULL) {
        dlog(LOG_WARN, "Failed to open hash file \"%s\", attempting to create", name);
        f = fopen_utf8(buf, "wb");
        if (f == NULL) {
            dlog(LOG_ERRSILENT, "Failed to open hash file \"%s\" for write!", name);
            return;
        }

        fwrite(&hash, sizeof(hash), 1, f);
        fclose(f);
        return;
    }

    size_t size = fread(&expected, sizeof(expected), 1, f);
    if (!size) {
        dlog(LOG_ERRSILENT, "Failed to read hash file \"%s\"!", name);
        fclose(f);
        return;
    }

    if (hash != expected) {
        dlog(LOG_INFO, "FAIL, expected: %016llx", expected);
        test_passed = false;
    }
}

static void finish_print()
{
    char exp[2048];
    char tmp[2048];
    file_path_append(exp, test.dir, "print.txt", sizeof(exp));
    file_path_append(tmp, test.dir, "print.txt.tmp", sizeof(tmp));

    FILE *expected = fopen_utf8(exp, "rb");
    if (expected == NULL) {
        dlog(LOG_WARN, "Failed to open print file \"%s\", attempting to create", exp);
        fclose(test.print);
        int err = rename(tmp, exp);
        if (err) {
            dlog(LOG_ERRSILENT, "Failed to rename print file!");
        }
        return;
    }

    fseek(test.print, 0, SEEK_SET);

    int line_no = 0;
    bool parsing = true;
    bool mismatch = false;
    while (parsing) {
        char *line1 = file_read_line(test.print);
        char *line2 = file_read_line(expected);
        line_no++;

        if (!line1 || !line2) {
            parsing = false;
            goto cleanup;
        }

        if (strcmp(line1, line2)) {
            dlog(LOG_INFO, "print FAIL, mismatch on line %d:", line_no);
            dlog(LOG_INFO, "  %s", line1);
            dlog(LOG_INFO, "  %s", line2);
            mismatch = true;
            test_passed = false;
            parsing = false;
        }

    cleanup:
        free(line1);
        free(line2);
    }

    if (!mismatch) {
        dlog(LOG_INFO, "print OK");
    }

    fclose(test.print);
    fclose(expected);

    remove(tmp);
}

static void test_finish(struct Machine *m) {
    if (test.docflags) {
        finish_hash(test.docflags, "docflags");
    }
    if (test.allflags) {
        finish_hash(test.allflags, "allflags");
    }
    if (test.cycles) {
        finish_hash(test.cycles, "cycles");
    }
    if (test.registers) {
        finish_hash(test.registers, "registers");
    }
    if (test.print) {
        finish_print();
    }

    machine_set_print_stream(NULL);

    machine_test_close();

    // FIXME: exit() kinda really hacky.. 
    // considering there's a bunch of stuff that doesnt really get deinit'd
    // and the fact cpu errors aren't handled at all...
    if (test_passed) {
        dlog(LOG_INFO, "test PASSED!\n");
        exit(0);
    } else {
        dlog(LOG_INFO, "test FAILED!\n");
        exit(-1);
    }
}

int machine_test_iterate(struct Machine *m)
{
    if (!test_running) return 0;

    if (test.cycles) {
        XXH64_update(test.cycles, &m->cpu.cycles, sizeof(m->cpu.cycles));
    }
    if (test.docflags) {
        uint8_t f = ~((1<<3) | (1<<5)) & m->cpu.regs.main.f;
        XXH64_update(test.docflags, &f, 1);
    }
    if (test.allflags) {
        uint8_t f = m->cpu.regs.main.f;
        XXH64_update(test.allflags, &f, 1);
    }
    if (test.registers) {
        XXH64_update(test.registers, &m->cpu.regs.main.a, 1);
        XXH64_update(test.registers, &m->cpu.regs.main.bc, 6);

        XXH64_update(test.registers, &m->cpu.regs.alt.a, 1);
        XXH64_update(test.registers, &m->cpu.regs.alt.bc, 6);

        XXH64_update(test.registers, &m->cpu.regs.ix, 10);

        XXH64_update(test.registers, &m->cpu.regs.im, 1);
    }

    if (test_condition(m)) {
        test_finish(m);
    }
}

void machine_test_close()
{
    if (test.dir) {
        free(test.dir);
        test.dir = NULL;
    }

    if (test.macro) {
        vector_free(test.macro);
        test.macro = NULL;
    }

    test_running = false;
}
