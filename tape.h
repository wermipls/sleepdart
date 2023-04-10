#pragma once

#include <stdint.h>
#include <stdbool.h>

enum TapeBlockType {
    TAPEBLOCK_STANDARD,
};

struct TapeBlock
{
    enum TapeBlockType type;
    size_t bytes;
    uint8_t *data;
};

enum TPBlockSection {
    TPSTATE_PILOT,
    TPSTATE_SYNC1,
    TPSTATE_SYNC2,
    TPSTATE_DATA,
    TPSTATE_PAUSE,
};

/* The tape and tape player structures are intended to be opaque objects,
 * not to be manipulated directly. There are many intricacies to how a tape 
 * can be stored or played, and the fields aren't really set in stone. */

typedef struct Tape
{
    size_t count;
    struct TapeBlock *blocks;
} Tape_t;

typedef struct TapePlayer {
    Tape_t *tape;

    size_t buffer_size;
    int32_t *buffer;

    // state of the buffer
    uint64_t buffer_start_cycle;    // absolute cycle the buffer is starting on
    uint64_t buffer_length_cycles;  // length of the current buffer data in cycles
    size_t buffer_pos;              // pulse buffer index 
    uint64_t buffer_pos_cycles;     // position in cycles relative to current buffer index

    // state of the processed tape
    size_t tape_block;
    enum TPBlockSection tape_block_section;
    // for data section -> size in bytes, pilot section -> pulse count
    uint32_t tape_section_len;
    // for data section -> byte index, pilot section -> current pulse
    uint32_t tape_section_pos;
    uint8_t tape_bit;              // for data section, indicates bit starting from MSB
    int tape_data_pulse_state;     // indicates if doing low (-1) or high (1) pulse

    // player state
    uint64_t position;
    bool paused;
    bool finished;
    bool error;
    bool end_of_tape;
} TapePlayer_t;

/* Initializes a tape from the provided tap file path.
 * Returns a pointer on success, 0 otherwise.
 * User is expected to call tape_free() when done using the object. */
Tape_t *tape_load_from_tap(char *path);

/* Deallocates the tape object and all underlying pointers. */
void tape_free(Tape_t *tape);

/* Initializes a tape player from the provided tape object.
 * Returns a pointer on success, 0 otherwise. 
 * User is expected to call tape_player_close() when done using the object. */
TapePlayer_t *tape_player_from_tape(Tape_t *tape);

/* Deinitializes the provided tape player.
 * User is responsible for disposing of the underlying tape object, if necessary. */
void tape_player_close(TapePlayer_t *player);

void tape_player_advance_cycles(TapePlayer_t *p, uint64_t cycles);
uint8_t tape_player_get_next_sample(TapePlayer_t *player, uint64_t cycles);
void tape_player_pause(TapePlayer_t *player, bool paused);
