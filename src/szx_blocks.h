#pragma once

#include <stdint.h>

#define PACKED __attribute__((__packed__))

typedef uint8_t byte;
typedef uint16_t word;
typedef uint32_t dword;


#define SZX_ZF_EILAST	1
#define SZX_ZF_HALTED	2

typedef struct PACKED SZXZ80Regs
{
  word af, bc, de, hl;
  word af1, bc1, de1, hl1;
  word ix, iy, sp, pc;
  byte i;
  byte r;
  byte iff1, iff2;
  byte im;
  dword cycles_start;
  byte hold_int_req_cycles;
  byte flags;
  word memptr;
} SZXZ80Regs_t;


#define SZX_RF_COMPRESSED 1

typedef struct PACKED SZXRAMPage
{
  word flags;
  byte page_no;
  byte data[1];
} SZXRAMPage_t;
