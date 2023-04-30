#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define SZX_MID_16K             0
#define SZX_MID_48K             1
#define SZX_MID_128K            2
#define SZX_MID_PLUS2           3
#define SZX_MID_PLUS2A          4
#define SZX_MID_PLUS3           5
#define SZX_MID_PLUS3E          6
#define SZX_MID_PENTAGON128     7
#define SZX_MID_TC2048          8
#define SZX_MID_TC2068          9
#define SZX_MID_SCORPION        10
#define SZX_MID_SE              11
#define SZX_MID_TS2068          12
#define SZX_MID_PENTAGON512     13
#define SZX_MID_PENTAGON1024    14
#define SZX_MID_NTSC48K         15
#define SZX_MID_128KE           16

#define SZX_MF_ALTERNATETIMINGS	1

struct SZXHeader
{
  uint32_t magic;
  uint8_t version_major;
  uint8_t version_minor;
  uint8_t machine_id;
  uint8_t flags;
};

struct SZXBlockHeader 
{
    uint32_t id;
    uint32_t size;
};

struct SZXBlock
{
    struct SZXBlockHeader header;
    uint8_t *data;
};

typedef struct SZX {
    struct SZXHeader header;
    size_t blocks;
    struct SZXBlock *block;
} SZX_t;

bool szx_is_valid_file(char *path);
SZX_t *szx_load_file(char *path);
int szx_save_file(SZX_t *szx, char *path);
void szx_free(SZX_t *szx);
