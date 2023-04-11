#pragma once

#include <stdint.h>
#include <stdbool.h>

enum SZXMachineID {
    ZXSTMID_16K          = 0,
    ZXSTMID_48K          = 1,
    ZXSTMID_128K         = 2,
    ZXSTMID_PLUS2        = 3,
    ZXSTMID_PLUS2A       = 4,
    ZXSTMID_PLUS3        = 5,
    ZXSTMID_PLUS3E       = 6,
    ZXSTMID_PENTAGON128  = 7,
    ZXSTMID_TC2048       = 8,
    ZXSTMID_TC2068       = 9,
    ZXSTMID_SCORPION     = 10,
    ZXSTMID_SE           = 11,
    ZXSTMID_TS2068       = 12,
    ZXSTMID_PENTAGON512  = 13,
    ZXSTMID_PENTAGON1024 = 14,
    ZXSTMID_NTSC48K      = 15,
    ZXSTMID_128KE        = 16,
};

#define ZXSTMF_ALTERNATETIMINGS	1

struct SZXHeader
{
  char magic[4];
  uint8_t version_major;
  uint8_t version_minor;
  enum SZXMachineID machine : 8;
  uint8_t flags;
};

bool szx_is_valid_file(char *path);
