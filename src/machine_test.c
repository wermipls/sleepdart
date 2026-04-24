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
#include <zlib.h>

#define CC_NO_SHORT_NAMES
#include "../external/cc.h"

#define STB_IMAGE_IMPLEMENTATION
#define STBI_WINDOWS_UTF8
#define STBI_ONLY_PNG
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#include "../external/stb_image.h"
#pragma GCC diagnostic pop

unsigned char *stbiw_custom_compress(unsigned char *data, int data_len, int *out_len, int quality)
{
    unsigned long sz = compressBound(data_len);
    unsigned char *buf = malloc(sz);
    if (buf) {
        if (compress2(buf, &sz, data, data_len, quality) != Z_OK) {
            free(buf);
            return NULL;
        }
        *out_len = sz;
    }

    return buf;
}

#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STBIW_WINDOWS_UTF8
#define STBIW_ZLIB_COMPRESS stbiw_custom_compress
#include "../external/stb_image_write.h"

enum StopCondition
{
    STOP_BREAKPOINT,
    STOP_FRAME,
};

const char *condition_str[] = {
    "breakpoint",
    "frame",
};

struct Screenshot {
    int width, height;
    uint8_t *data;
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
    bool test_screenshot;
    FILE *print;
    cc_map(uint64_t, struct Screenshot) screenshots;
    cc_set(uint64_t) screenshot_frames;

    KeyboardMacro_t *macro;
};

static struct CfgField test_fields[] = {
    { "file",              CFG_STR, NULL },
    { "stop-condition",    CFG_STR, NULL },
    { "stop-value",        CFG_INT, NULL },
    { "scope",             CFG_STR, NULL },
    { "macro",             CFG_STR, NULL },
    { "screenshot-frames", CFG_STR, NULL },
};

static CfgData_t testcfg = {
    .data = test_fields,
    .len = sizeof(test_fields) / sizeof(struct CfgField)
};

static bool test_running = false;
static bool test_passed = true;
static struct MachineTest test = { 0 };

int machine_test_open(const char *path)
{
    cc_init(&test.screenshot_frames);
    cc_init(&test.screenshots);

    if (path == NULL) {
        return -1;
    }

    char *config_path = file_path_append(NULL, path, "sleepdart-test.ini");
    if (!config_path) {
        dlog(LOG_ERRSILENT, "%s: path append fail", __func__);
        return -1;
    }
    int err = config_load_file(&testcfg, config_path);
    if (err) {
        dlog(LOG_ERRSILENT, "Failed to open test file \"%s\"", config_path);
        return -2;
    }
    free(config_path);

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
        char *file_path = file_path_append(NULL, path, file);
        machine_open_file(file_path);
        free(file);
        free(file_path);
    }

    char *condition = config_get_str(&testcfg, "stop-condition");

    if (condition == NULL) {
        dlog(LOG_ERRSILENT, "Missing stop-condition parameter in config file");
        return -4;
    }

    bool match = 0;
    for (size_t i = 0; i < sizeof(condition_str) / sizeof(char *); i++) {
        if (strcmp(condition, condition_str[i]) == 0) {
            test.stop_condition = i;
            match = true;
            break;
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
        dlog(LOG_ERRSILENT, "Test scope not defined");
        return -1;
    } else {
        char *last;
        char *token;
        char *str = scope;
        bool scope_defined = false;
        while ((token = strtok_r(str, " ", &last)) != NULL) {
            if (strcmp("print", token) == 0) {
                test.test_print = true;
                scope_defined = true;
            } else if (strcmp("screenshot", token) == 0) {
                test.test_screenshot = true;
                scope_defined = true;
            } else {
                dlog(LOG_ERRSILENT, "Unknown test scope \"%s\"", token);
                free(scope);
                return -7;
            }
            str = NULL;
        }
        if (!scope_defined) {
            dlog(LOG_ERRSILENT, "Test scope not defined");
            return -1;
        }
        free(scope);
    }

    if (test.test_print) {
        char *print_path = file_path_append(NULL, path, "print.txt.tmp");
        if (!print_path) {
            dlog(LOG_ERRSILENT, "%s: path append fail", __func__);
            return -1;
        }
        test.print = fopen_utf8(print_path, "wb+");
        if (test.print == NULL) {
            dlog(LOG_ERR, "Failed to open file \"%s\" for write", print_path);
            free(print_path);
            return -8;
        }
        machine_set_print_stream(test.print);
        free(print_path);
    }
    if (test.test_screenshot) {
        char *frames_str = config_get_str(&testcfg, "screenshot-frames");
        if (!frames_str) {
            dlog(LOG_ERRSILENT, "Screenshot scope enabled but no frames specified");
            return -1;
        }

        bool frames_defined = false;

        char *last;
        char *token;
        char *str = frames_str;
        while ((token = strtok_r(str, " ", &last)) != NULL) {
            uint64_t frame = strtoull(token, NULL, 10);
            if (frame) {
                cc_insert(&test.screenshot_frames, frame);
                frames_defined = true;
            }
            str = NULL;
        }

        if (!frames_defined) {
            dlog(LOG_ERRSILENT, "Screenshot scope enabled but no frames specified");
            return -1;
        }
    }

    // we must enforce a specific palette, otherwise screenshot tests may fail.
    palette_set_by_name("default.raw");

    // ensure savestate (if any) is loaded before setting macro.
    machine_process_events();

    char *macro = config_get_str(&testcfg, "macro");
    if (macro) {
        char *macro_path = file_path_append(NULL, path, macro);
        free(macro);
        if (!macro_path) {
            dlog(LOG_ERRSILENT, "%s: path append fail", __func__);
            return -1;
        }

        test.macro = keyboard_macro_parse(macro_path);
        if (test.macro == NULL) {
            dlog(LOG_WARN, "Failed to parse macro file \"%s\"", macro_path);
            free(macro_path);
            return -8;
        }
        keyboard_macro_play(test.macro, vector_len(test.macro));
        free(macro_path);
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

static void finish_print()
{
    char *exp_path = file_path_append(NULL, test.dir, "print.txt");
    char *tmp_path = file_path_append(NULL, test.dir, "print.txt.tmp");

    if (!exp_path || !tmp_path) {
        dlog(LOG_ERRSILENT, "%s: path append fail", __func__);
        free(exp_path);
        free(tmp_path);
        return;
    }

    FILE *expected = fopen_utf8(exp_path, "rb");
    if (expected == NULL) {
        dlog(LOG_WARN, "Failed to open print file \"%s\", attempting to create", exp_path);
        fclose(test.print);
        int err = rename(tmp_path, exp_path);
        if (err) {
            dlog(LOG_ERRSILENT, "Failed to rename print file!");
        }
        free(exp_path);
        free(tmp_path);
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

    remove(tmp_path);

    free(exp_path);
    free(tmp_path);
}

static void finish_screenshot()
{
    cc_for_each(&test.screenshot_frames, el) {
        uint64_t frame_n = *el;
        struct Screenshot *screenshot = cc_get(&test.screenshots, frame_n);
        if (!screenshot) {
            dlog(LOG_INFO, "FAIL, screenshot expected on frame %lu but none was made", frame_n);
            test_passed = false;
            continue;
        }

        char exp_name[128];
        snprintf(exp_name, sizeof(exp_name), "screen%05lu.png", frame_n);
        char *exp_path = file_path_append(NULL, test.dir, exp_name);
        if (!exp_path) {
            dlog(LOG_ERRSILENT, "%s: path append fail", __func__);
            test_passed = false;
            continue;
        }

        struct Screenshot exp_screenshot;
        exp_screenshot.data = stbi_load(exp_path, &exp_screenshot.width, &exp_screenshot.height, NULL, 3);
        if (!exp_screenshot.data) {
            dlog(LOG_WARN, "Failed to open reference screenshot \"%s\", attempting to create", exp_path);
            if (!stbi_write_png(exp_path, screenshot->width, screenshot->height, 3, screenshot->data, 0)) {
                dlog(LOG_ERRSILENT, "Failed to write image \"%s\"", exp_path);
            }
            free(exp_path);
            continue;
        }
        free(exp_path);

        if (screenshot->width != exp_screenshot.width || screenshot->height != exp_screenshot.height) {
            dlog(LOG_INFO, "FAIL, screenshot resolution mismatch - frame %lu is %dx%d, reference is %dx%d",
                 frame_n, screenshot->width, screenshot->height, exp_screenshot.width, exp_screenshot.height);
            test_passed = false;
            free(exp_screenshot.data);
            continue;
        }

        if (memcmp(screenshot->data, exp_screenshot.data, screenshot->width*screenshot->height*3)) {
            dlog(LOG_INFO, "FAIL, screenshot frame %lu does not match", frame_n);
            test_passed = false;

            char fail_name[128];
            snprintf(fail_name, sizeof(fail_name), "screen%05lu.FAIL.png", frame_n);
            char *fail_path = file_path_append(NULL, test.dir, fail_name);
            if (fail_path && stbi_write_png(fail_path, screenshot->width, screenshot->height, 3, screenshot->data, 0)) {
                dlog(LOG_INFO, "  screenshot saved to \"%s\"", fail_path);
                free(fail_path);
            }
        }

        free(exp_screenshot.data);
    }
}

static void test_finish(struct Machine *m)
{
    if (test.print) {
        finish_print();
    }
    if (test.test_screenshot) {
        finish_screenshot();
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

void machine_test_iterate(struct Machine *m)
{
    if (!test_running) return;

    if (test_condition(m)) {
        test_finish(m);
    }
}

void machine_test_iterate_frame(struct Machine *m)
{
    if (!test_running) return;

    if (test.test_screenshot && cc_get(&test.screenshot_frames, m->frames)) {
        struct Screenshot screenshot = {0};
        screenshot.data = malloc(BUFFER_LEN*3);
        if (screenshot.data) {
            screenshot.width  = BUFFER_WIDTH;
            screenshot.height = BUFFER_HEIGHT;
            static_assert(BUFFER_LEN*3 == sizeof(ula_buffer), "ula buffer must be 24bpp.");
            memcpy(screenshot.data, ula_buffer, BUFFER_LEN * 3);
            cc_insert(&test.screenshots, m->frames, screenshot);
        }
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

    cc_for_each(&test.screenshots, screenshot) {
        free(screenshot->data);
    }

    cc_cleanup(&test.screenshots);
    cc_cleanup(&test.screenshot_frames);

    test_running = false;
}
