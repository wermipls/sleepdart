#include "tape.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "log.h"

struct StandardTapeHeader
{
    uint8_t type;
    char filename[10];
    uint16_t length;
    uint16_t param1;
    uint16_t param2;
};

void tape_free(Tape_t *tape)
{
    if (tape == NULL) return;

    if (tape->blocks != NULL) {
        for (size_t i = 0; i < tape->count; i++) {
            free(tape->blocks[i].data);
        }

        free(tape->blocks);
    }
    free(tape);
}

Tape_t *tape_load_from_tap(char *path)
{
    // .tap is a fairly simple format.
    // It consists of an arbitrary amount of blocks
    // beginning with a uint16le field indicating the data size in bytes,
    // followed by the actual block data (in standard Spectrum ROM format).
    // e.g. 13 00 [19 data bytes] 00 10 [256 data bytes] ...

    FILE *f = fopen(path, "rb");

    if (f == NULL) {
        goto error_fopen;
    }

    Tape_t *tape = malloc(sizeof(Tape_t));
    if (tape == NULL) {
        dlog(LOG_ERRSILENT, "Failed to allocate memory for tape struct");
        goto error_fclose;
    }
    tape->count = 0;

    // need to find the amount of blocks in the tap
    // so we can allocate the correct amount of memory later on
    size_t tap_blocks = 0;

    while (!feof(f)) {
        uint16_t size;
        size_t bytes = fread(&size, 1, sizeof(size), f);

        if (bytes != sizeof(size)) {
            if (feof(f) && bytes == 0) {
                break;
            }
            dlog(LOG_ERRSILENT, "Expected to read %d bytes, got %d",
                                sizeof(size), bytes);
            goto error_free_fclose;
        }

        if (size == 0) {
            dlog(LOG_ERRSILENT, "Declared block size of 0; expected > 0");
            goto error_free_fclose;
        }

        tap_blocks += 1;
        fseek(f, size, SEEK_CUR);
    }

    tape->blocks = malloc(sizeof(struct TapeBlock) * tap_blocks);
    if (tape->blocks == NULL) {
        dlog(LOG_ERRSILENT, "Failed to allocate memory for tape block list");
        goto error_free_fclose;
    }

    fseek(f, 0, SEEK_SET);

    while (!feof(f)) {
        uint16_t size;
        size_t bytes = fread(&size, 1, sizeof(size), f);

        if (bytes != sizeof(size)) {
            if (feof(f) && bytes == 0) {
                break;
            }
            dlog(LOG_ERRSILENT, "Expected to read %d bytes, got %d",
                                sizeof(size), bytes);
            goto error_free_fclose;
        }

        if (size == 0) {
            dlog(LOG_ERRSILENT, "Declared block size of 0; expected > 0");
            goto error_free_fclose;
        }

        struct TapeBlock block;
        block.type = TAPEBLOCK_STANDARD; // .tap blocks are standard by definition
        block.bytes = size;
        block.data = malloc(block.bytes);

        if (block.data == NULL) {
            dlog(LOG_ERRSILENT, "Failed to allocate memory for tape block struct");
            goto error_free_fclose;
        }

        bytes = fread(block.data, 1, block.bytes, f);
        if (bytes != block.bytes) {
            dlog(LOG_ERRSILENT, "Expected to read %d bytes, got %d", 
                                block.bytes, bytes);
            free(block.data);
            goto error_free_fclose;
        }

        tape->blocks[tape->count] = block;
        tape->count++;
    }

    dlog(LOG_INFO, "Loaded %d tape blocks from %s", tape->count, path);
    return tape;

error_free_fclose:
    tape_free(tape);
error_fclose:
    fclose(f);
error_fopen:
    dlog(LOG_ERR, "Failed to load tap file \"%s\"", path);
    return NULL;
}

TapePlayer_t *tape_player_allocate(size_t bufsize)
{
    TapePlayer_t *p = calloc(1, sizeof(TapePlayer_t));
    if (p == NULL) {
        dlog(LOG_ERRSILENT, "Failed to allocate memory for tape player");
        return NULL;
    }

    p->buffer_size = bufsize;
    p->buffer = calloc(bufsize, sizeof(*p->buffer));

    if (p->buffer == NULL) {
        dlog(LOG_ERRSILENT, "Failed to allocate memory for tape player buffer");
        free(p);
        return NULL;
    }

    return p;
}

int player_init_block_state(TapePlayer_t *p)
{
    int block_i = p->tape_block;
    if (block_i >= p->tape->count) {
        return -1;
    }

    struct TapeBlock *block = &p->tape->blocks[block_i];

    if (block->type != TAPEBLOCK_STANDARD) {
        return -2;
    }

    p->tape_block_section = TPSTATE_PILOT;

    uint8_t flag = block->data[0];
    if (flag == 0) {                    // header
        p->tape_section_len = 8063;
    } else if (flag == 0xFF) {          // data
        p->tape_section_len = 3223;
    }

    p->tape_section_pos = 0;

    return 0;
}

void render_buffer(TapePlayer_t *p);

TapePlayer_t *tape_player_from_tape(Tape_t *tape)
{
    TapePlayer_t *p = tape_player_allocate(4096);
    if (p == NULL) {
        return NULL;
    }

    p->tape = tape;
    p->position = 0;

    int err = player_init_block_state(p);
    if (err) {
        tape_player_close(p);
        return NULL;
    }

    render_buffer(p);

    return p;
}

void tape_player_close(TapePlayer_t *player) {
    if (player == NULL) return;

    free(player->buffer);
    free(player);
}

static inline int advance_block_section(TapePlayer_t *p)
{
    struct TapeBlock *block = &p->tape->blocks[p->tape_block];

    switch (p->tape_block_section) 
    {
    case TPSTATE_PILOT:
        p->tape_block_section = TPSTATE_SYNC1;
        break;
    case TPSTATE_SYNC1:
        p->tape_block_section = TPSTATE_SYNC2;
        break;
    case TPSTATE_SYNC2:
        p->tape_block_section = TPSTATE_DATA;
        p->tape_section_len = block->bytes;
        p->tape_section_pos = 0;
        p->tape_bit = 0;
        p->tape_data_pulse_state = -1;
        break;
    case TPSTATE_DATA:
        p->tape_block_section = TPSTATE_PAUSE;
        break;
    case TPSTATE_PAUSE:
        if (p->tape_block + 1 < p->tape->count) {
            p->tape_block++;
            return player_init_block_state(p);
        } else {
            p->tape_block_section = TPSTATE_END;
        }
        break;
    case TPSTATE_END:
        break;
    }

    return 0;
}

int render_buffer_block_section(TapePlayer_t *p) {
    if (p->buffer_pos >= p->buffer_size) {
        return -1;
    }

    int pulses = 0;

    int32_t pilot_pulselen = 2168;
    int32_t sync1_pulselen = 667;
    int32_t sync2_pulselen = 735;
    int32_t pause_pulselen = 3500000;
    int32_t datalo_pulselen = 855;
    int32_t datahi_pulselen = 1710;

    switch (p->tape_block_section) 
    {
    case TPSTATE_PILOT:
        while (p->tape_section_pos < p->tape_section_len) {
            if (p->buffer_pos < p->buffer_size) {
                int is_low = p->tape_section_pos & 1;
                p->buffer[p->buffer_pos] = is_low ? -pilot_pulselen 
                                                  :  pilot_pulselen;
                p->tape_section_pos++;
                p->buffer_pos++;
                pulses++;
            } else {
                return pulses;
            }
        }
        break;
    case TPSTATE_SYNC1:
        p->buffer[p->buffer_pos] = -sync1_pulselen;
        p->buffer_pos++;
        pulses++;
        break;
    case TPSTATE_SYNC2:
        p->buffer[p->buffer_pos] = sync2_pulselen;
        p->buffer_pos++;
        pulses++;
        break;
    case TPSTATE_DATA:
        while (p->tape_section_pos < p->tape_section_len) {
            uint8_t byte = p->tape->blocks[p->tape_block].data[p->tape_section_pos];
            while (1) {
                if (p->buffer_pos >= p->buffer_size) {
                    return pulses;
                }

                int bit = byte & (1<<(7 - p->tape_bit));
                bit = bit ? datahi_pulselen : datalo_pulselen;

                p->buffer[p->buffer_pos] = bit * p->tape_data_pulse_state;
                p->buffer_pos++;
                pulses++;

                if (p->tape_data_pulse_state == 1) {
                    p->tape_bit++;
                    p->tape_data_pulse_state = -1;
                    if (p->tape_bit >= 8) {
                        p->tape_bit = 0;
                        p->tape_section_pos++;
                        break;
                    }
                } else {
                    p->tape_data_pulse_state = 1;
                }
            }
        }
        break;
    case TPSTATE_PAUSE:
        p->buffer[p->buffer_pos] = -pause_pulselen;
        p->buffer_pos++;
        pulses++;
        break;
    case TPSTATE_END:
        while (p->buffer_pos < p->buffer_size) {
            p->buffer[p->buffer_pos] = 0;
            p->buffer_pos++;
        }
        return pulses;
    }

    int err = advance_block_section(p);
    if (err) {
        return -1;
    }

    return pulses;
}

void render_buffer(TapePlayer_t *p)
{
    if (p->finished || p->error) return;

    p->buffer_pos = 0;

    size_t pulses_total = 0;
    int pulses;
    do {
        pulses = render_buffer_block_section(p);
        pulses_total += pulses;
    } while (p->buffer_pos < p->buffer_size && pulses > 0);

    p->buffer_pos = 0;

    if (pulses < 0) {
        p->error = true;
        return;
    }

    uint64_t cyc = 0;

    for (size_t i = 0; i < p->buffer_size; i++) {
        cyc += abs(p->buffer[i]);
    }

    p->buffer_start_cycle += p->buffer_length_cycles;
    p->buffer_length_cycles = cyc;
}

void tape_player_advance_cycles(TapePlayer_t *p, uint64_t cycles)
{
    if (p == NULL) return;

    if (cycles == 0 || p->paused || p->finished || p->error) return;

    if (p->buffer_pos >= p->buffer_size) {
        render_buffer(p);
    }

    p->buffer_pos_cycles += cycles;
    int32_t pulse_len = abs(p->buffer[p->buffer_pos]);

    while (p->buffer_pos_cycles >= pulse_len) {
        if (pulse_len == 0) {
            p->finished = true;
            break;
        }

        p->buffer_pos_cycles -= pulse_len;
        p->buffer_pos++;

        if (p->buffer_pos >= p->buffer_size) {
            render_buffer(p);
        }

        pulse_len = abs(p->buffer[p->buffer_pos]);
    }

    p->position += cycles;
}

uint8_t tape_player_get_next_sample(TapePlayer_t *player, uint64_t cycles) {
    if (player == NULL) return;
    
    if (player->paused || player->finished || player->error) {
        return 0;
    }

    if (player->buffer_pos >= player->buffer_size) {
        render_buffer(player);
    }

    tape_player_advance_cycles(player, cycles);

    int32_t pulse = player->buffer[player->buffer_pos];
    if (pulse > 0) {
        return 1;
    } else {
        if (pulse == 0) {
            player->finished = true;
        }
        return 0;
    }
}

void tape_player_pause(TapePlayer_t *player, bool paused)
{
    if (player == NULL) return;

    player->paused = paused;
}
