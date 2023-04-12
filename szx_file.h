#pragma once

#include <stdint.h>
#include <stdbool.h>

#define ZXSTMID_16K             0
#define ZXSTMID_48K             1
#define ZXSTMID_128K            2
#define ZXSTMID_PLUS2           3
#define ZXSTMID_PLUS2A          4
#define ZXSTMID_PLUS3           5
#define ZXSTMID_PLUS3E          6
#define ZXSTMID_PENTAGON128     7
#define ZXSTMID_TC2048          8
#define ZXSTMID_TC2068          9
#define ZXSTMID_SCORPION        10
#define ZXSTMID_SE              11
#define ZXSTMID_TS2068          12
#define ZXSTMID_PENTAGON512     13
#define ZXSTMID_PENTAGON1024    14
#define ZXSTMID_NTSC48K         15
#define ZXSTMID_128KE           16

#define ZXSTMF_ALTERNATETIMINGS	1

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
void szx_free(SZX_t *szx);
