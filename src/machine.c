#include "machine.h"
#include <string.h>
#include "machine_test.h"
#include "machine_hooks.h"
#include "keyboard_macro.h"
#include "file.h"
#include "szx_state.h"
#include "log.h"
#include "input_sdl.h"
#include "video_sdl.h"
#include "audio_sdl.h"
#include "dsp.h"
#include "hotkeys.h"

static Machine_t *m_cur = NULL;
static bool file_open;
static bool file_save;
static char file_open_path[2048];
static char file_save_path[2048];

const struct MachineTiming machine_timing_zx48k = {
    .clock_hz = 3500000,

    .t_firstpx = 14336,
    .t_scanline = 224,
    .t_screen = 128,
    .t_frame = 224 * 312,
    .t_eightpx = 4,
    .t_int_hold = 32,
};

void machine_set_current(Machine_t *machine)
{
    m_cur = machine;
}

int machine_init(Machine_t *machine, enum MachineType type)
{
    if (machine == NULL) {
        return -1;
    }

    if (type != MACHINE_ZX48K) {
        return -2;
    }

    machine->type = type;
    machine->timing = machine_timing_zx48k;

    memory_init(&machine->memory);

    char path[2048];
    file_path_append(path, file_get_basedir(), "rom/48.rom", sizeof(path));
    memory_load_rom_16k(&machine->memory, path);

    machine->cpu.ctx = machine;
    cpu_init(&machine->cpu);

    machine->tape = NULL;
    machine->player = NULL;
    machine->frames = 0;
    machine->reset_pending = false;

    // FIXME: hack
    ula_init(machine);

    return 0;
}

void machine_deinit(Machine_t *machine)
{
    if (machine->player) {
        tape_player_close(machine->player);
        machine->player = NULL;
    }

    if (machine->tape) {
        tape_free(machine->tape);
        machine->tape = NULL;
    }
}

void machine_reset() 
{
    if (m_cur == NULL) return;
    m_cur->reset_pending = true;
}

void machine_process_events()
{
    if (m_cur == NULL) return;

    if (file_open) {
        file_open = false;
        enum FileType ft = file_detect_type(file_open_path);

        switch (ft)
        {
        case FTYPE_TAP:
            if (m_cur->tape != NULL) {
                tape_free(m_cur->tape);
            }
            if (m_cur->player != NULL) {
                tape_player_close(m_cur->player);
            }

            m_cur->tape = tape_load_from_tap(file_open_path);
            m_cur->player = tape_player_from_tape(m_cur->tape);
            tape_player_pause(m_cur->player, true);
            break;
        case FTYPE_SZX: ;
            SZX_t *szx = szx_load_file(file_open_path);
            if (szx != NULL) {
                szx_state_load(szx, m_cur);
                szx_free(szx);
            }
            break;
        default:
            dlog(LOG_ERR, "Unrecognized input file \"%s\"", file_open_path);
        }
    }

    if (file_save) {
        SZX_t *szx = szx_state_save(m_cur);
        if (szx != NULL) {
            szx_save_file(szx, file_save_path);
            szx_free(szx);
        }
        file_save = false;
    }

    if (m_cur->reset_pending) {
        cpu_init(&m_cur->cpu);
        ay_reset(m_cur->ay);
        m_cur->reset_pending = false;
    }

    if (palette_has_changed()) {
        Palette_t *pal = palette_load_current();
        if (pal) {
            ula_set_palette(pal);
            palette_free(pal);
        }
    }
}

void machine_open_file(char *path)
{
    if (path == NULL) return;
    
    strncpy(file_open_path, path, sizeof(file_open_path)-1);
    file_open_path[sizeof(file_open_path)-1] = 0;
    file_open = true;
}

void machine_save_file(char *path)
{
    if (path == NULL) return;
    
    strncpy(file_save_path, path, sizeof(file_save_path)-1);
    file_save_path[sizeof(file_save_path)-1] = 0;
    file_save = true;
}

static char *get_quicksave_path()
{
    static char buf[2048] = { 0 };
    if (buf[0] == 0) {
        file_path_append(buf, file_get_basedir(), "quicksave.szx", sizeof(buf));
    }
    return buf;
}

void machine_save_quick()
{
    machine_save_file(get_quicksave_path());
}

void machine_load_quick()
{
    machine_open_file(get_quicksave_path());
}

void machine_toggle_tape_playback()
{
    if (m_cur == NULL) return;
    if (m_cur->player == NULL) return;

    tape_player_pause(m_cur->player, !m_cur->player->paused);
}

int machine_do_cycles()
{
    while (!m_cur->cpu.error) {
        if (m_cur->cpu.cycles < m_cur->timing.t_int_hold) {
            cpu_fire_interrupt(&m_cur->cpu);
        }

        machine_process_hooks(m_cur);
        machine_test_iterate(m_cur);

        cpu_do_cycles(&m_cur->cpu);

        if (m_cur->cpu.cycles >= m_cur->timing.t_frame) {
            m_cur->cpu.cycles -= m_cur->timing.t_frame;
            m_cur->frames++;

            ay_process_frame(m_cur->ay);
            beeper_process_frame(&m_cur->beeper);
            dsp_mix_buffers_mono_to_stereo(m_cur->ay->buf, m_cur->beeper.buf, m_cur->ay->buf_len);

            audio_sdl_queue(m_cur->ay->buf, m_cur->ay->buf_len * sizeof(float));

            ula_draw_frame(m_cur);

            video_sdl_draw_rgb24_buffer(ula_buffer, sizeof(ula_buffer));

            keyboard_macro_process();

            input_sdl_copy_old_state();
            int quit = input_sdl_update();
            if (quit) return -2;

            hotkeys_process();
            machine_process_events();
            return 0;
        }
    }

    return -1;
}
