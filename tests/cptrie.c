/*
 * Copyright (c) 2019-2020 MD Iftakharul Islam (Tamim) <tamim@csebuet.org>
 * All rights reserved.
 */
#include <stdint.h>

#define SIZE16 1024
#define SIZE24 240
#define SIZE32 240
#define SIZE40 240
#define SIZE48 240
#define SIZE56 240
#define SIZE64 240
#define SIZE72 240
#define SIZE80 240
#define SIZE88 240
#define SIZE96 240
#define SIZE104 240
#define SIZE112 240
#define SIZE120 240
#define SIZE128 240

#define N_CNT 664500

#define MSK 0X8000000000000000ULL

//types and constants used in the functions below
//uint64_t is an unsigned 64-bit integer variable type (defined in C99 version of C language)
#define m1 0x5555555555555555 //binary: 0101...
#define m2 0x3333333333333333 //binary: 00110011..
#define m4 0x0f0f0f0f0f0f0f0f //binary:  4 zeros,  4 ones ...
#define m8 0x00ff00ff00ff00ff //binary:  8 zeros,  8 ones ...
#define m16 0x0000ffff0000ffff //binary: 16 zeros, 16 ones ...
#define m32 0x00000000ffffffff //binary: 32 zeros, 32 ones
#define h01 0x0101010101010101 //the sum of 256 to the power of 0,1,2,3...

///**
// * The macro is created based the following function. It's tested to be working properly. 
//inline uint8_t POPCNT64 (int64_t w)
//{
//  w -= (w >> 1) & 0x5555555555555555ul;
//  w =  (w & 0x3333333333333333ul) + ((w >> 2) & 0x3333333333333333ul);
//  w =  (w + (w >> 4)) & 0x0f0f0f0f0f0f0f0ful;
//  return (w * 0x0101010101010101ul) >> 56;
//}
//*/
#define POPCNT64(w) ((((((((w) - (((w) >> 1) & 0x5555555555555555ul)) & 0x3333333333333333ul) + ((((w) - (((w) >> 1) & 0x5555555555555555ul)) >> 2) & 0x3333333333333333ul)) + (((((w) - (((w) >> 1) & 0x5555555555555555ul)) & 0x3333333333333333ul) + ((((w) - (((w) >> 1) & 0x5555555555555555ul)) >> 2) & 0x3333333333333333ul)) >> 4)) & 0x0f0f0f0f0f0f0f0ful) * 0x0101010101010101ul) >> 56)

/*
 * The macro is created based the following function. It's tested to be working properly. 
int popcount64b(uint64_t x)
{
    x -= (x >> 1) & m1;             //put count of each 2 bits into those 2 bits
    x = (x & m2) + ((x >> 2) & m2); //put count of each 4 bits into those 4 bits 
    x = (x + (x >> 4)) & m4;        //put count of each 8 bits into those 8 bits 
    x += x >>  8;  //put count of each 16 bits into their lowest 8 bits
    x += x >> 16;  //put count of each 32 bits into their lowest 8 bits
    x += x >> 32;  //put count of each 64 bits into their lowest 8 bits
    return x & 0x7f;
}
*/
//#define POPCNT64(x) ((((((((((x) - (((x) >> 1) & m1)) & m2) + ((((x) - (((x) >> 1) & m1)) >> 2) & m2)) + (((((x) - (((x) >> 1) & m1)) & m2) + ((((x) - (((x) >> 1) & m1)) >> 2) & m2)) >> 4)) & m4) + (((((((x) - (((x) >> 1) & m1)) & m2) + ((((x) - (((x) >> 1) & m1)) >> 2) & m2)) + (((((x) - (((x) >> 1) & m1)) & m2) + ((((x) - (((x) >> 1) & m1)) >> 2) & m2)) >> 4)) & m4) >>  8)) + ((((((((x) - (((x) >> 1) & m1)) & m2) + ((((x) - (((x) >> 1) & m1)) >> 2) & m2)) + (((((x) - (((x) >> 1) & m1)) & m2) + ((((x) - (((x) >> 1) & m1)) >> 2) & m2)) >> 4)) & m4) + (((((((x) - (((x) >> 1) & m1)) & m2) + ((((x) - (((x) >> 1) & m1)) >> 2) & m2)) + (((((x) - (((x) >> 1) & m1)) & m2) + ((((x) - (((x) >> 1) & m1)) >> 2) & m2)) >> 4)) & m4) >>  8)) >> 16)) + (((((((((x) - (((x) >> 1) & m1)) & m2) + ((((x) - (((x) >> 1) & m1)) >> 2) & m2)) + (((((x) - (((x) >> 1) & m1)) & m2) + ((((x) - (((x) >> 1) & m1)) >> 2) & m2)) >> 4)) & m4) + (((((((x) - (((x) >> 1) & m1)) & m2) + ((((x) - (((x) >> 1) & m1)) >> 2) & m2)) + (((((x) - (((x) >> 1) & m1)) & m2) + ((((x) - (((x) >> 1) & m1)) >> 2) & m2)) >> 4)) & m4) >>  8)) + ((((((((x) - (((x) >> 1) & m1)) & m2) + ((((x) - (((x) >> 1) & m1)) >> 2) & m2)) + (((((x) - (((x) >> 1) & m1)) & m2) + ((((x) - (((x) >> 1) & m1)) >> 2) & m2)) >> 4)) & m4) + (((((((x) - (((x) >> 1) & m1)) & m2) + ((((x) - (((x) >> 1) & m1)) >> 2) & m2)) + (((((x) - (((x) >> 1) & m1)) & m2) + ((((x) - (((x) >> 1) & m1)) >> 2) & m2)) >> 4)) & m4) >>  8)) >> 16)) >> 32)) & 0x7f)

/**
 * We don't support struct.
 * We don't support flag variable.
 */
uint64_t cptrie(uint64_t c16_bitmap[SIZE16], uint32_t c16_popcnt[SIZE16],
        uint64_t b16_bitmap[SIZE16], uint32_t b16_popcnt[SIZE16],
        uint64_t c24_bitmap[SIZE24], uint32_t c24_popcnt[SIZE24],
        uint64_t b24_bitmap[SIZE24], uint32_t b24_popcnt[SIZE24],
        uint64_t c32_bitmap[SIZE32], uint32_t c32_popcnt[SIZE32],
        uint64_t b32_bitmap[SIZE32], uint32_t b32_popcnt[SIZE32],
        uint64_t c40_bitmap[SIZE40], uint32_t c40_popcnt[SIZE40],
        uint64_t b40_bitmap[SIZE40], uint32_t b40_popcnt[SIZE40],
        uint64_t c48_bitmap[SIZE48], uint32_t c48_popcnt[SIZE48],
        uint64_t b48_bitmap[SIZE48], uint32_t b48_popcnt[SIZE48],
        uint64_t c56_bitmap[SIZE56], uint32_t c56_popcnt[SIZE56],
        uint64_t b56_bitmap[SIZE56], uint32_t b56_popcnt[SIZE56],
        uint64_t c64_bitmap[SIZE64], uint32_t c64_popcnt[SIZE64],
        uint64_t b64_bitmap[SIZE64], uint32_t b64_popcnt[SIZE64],
        uint64_t c72_bitmap[SIZE72], uint32_t c72_popcnt[SIZE72],
        uint64_t b72_bitmap[SIZE72], uint32_t b72_popcnt[SIZE72],
        uint64_t c80_bitmap[SIZE80], uint32_t c80_popcnt[SIZE80],
        uint64_t b80_bitmap[SIZE80], uint32_t b80_popcnt[SIZE80],
        uint64_t c88_bitmap[SIZE88], uint32_t c88_popcnt[SIZE88],
        uint64_t b88_bitmap[SIZE88], uint32_t b88_popcnt[SIZE88],
        uint64_t c96_bitmap[SIZE96], uint32_t c96_popcnt[SIZE96],
        uint64_t b96_bitmap[SIZE96], uint32_t b96_popcnt[SIZE96],
        uint64_t c104_bitmap[SIZE104], uint32_t c104_popcnt[SIZE104],
        uint64_t b104_bitmap[SIZE104], uint32_t b104_popcnt[SIZE104],
        uint64_t c112_bitmap[SIZE112], uint32_t c112_popcnt[SIZE112],
        uint64_t b112_bitmap[SIZE112], uint32_t b112_popcnt[SIZE112],
        uint64_t c120_bitmap[SIZE120], uint32_t c120_popcnt[SIZE120],
        uint64_t b120_bitmap[SIZE120], uint32_t b120_popcnt[SIZE120],
        uint64_t b128_bitmap[SIZE128], uint32_t b128_popcnt[SIZE128],
        uint8_t leafN[N_CNT], uint64_t ip1, uint64_t ip2) {
  uint64_t n_idx;
  uint32_t off;
  uint32_t idx, idx_sail;
  uint32_t ck_idx;

  idx_sail = ip1 >> 48;
  idx = idx_sail >> 6;
  off = idx_sail & 63;

  if (c16_bitmap[idx] & (MSK >> off)) {
    ck_idx = c16_popcnt[idx] + POPCNT64(c16_bitmap[idx] >> (64 - off));
    idx_sail = (ck_idx << 8) + ((ip1 >> 40) & 0XFF);
    idx = idx_sail >> 6;
    off = idx_sail & 63;
    if (c24_bitmap[idx] & (MSK >> off)) {
      ck_idx = c24_popcnt[idx] + POPCNT64(c24_bitmap[idx] >> (64 - off));
      idx_sail = (ck_idx << 8) + ((ip1 >> 32) & 0XFF);
      idx = idx_sail >> 6;
      off = idx_sail & 63;
      if (c32_bitmap[idx] & (MSK >> off)) {
        ck_idx = c32_popcnt[idx] + POPCNT64(c32_bitmap[idx] >> (64 - off));
        idx_sail = (ck_idx << 8) + ((ip1 >> 24) & 0XFF);
        idx = idx_sail >> 6;
        off = idx_sail & 63;
        if (c40_bitmap[idx] & (MSK >> off)) {
          ck_idx = c40_popcnt[idx] + POPCNT64(c40_bitmap[idx] >> (64 - off));
          idx_sail = (ck_idx << 8) + ((ip1 >> 16) & 0XFF);
          idx = idx_sail >> 6;
          off = idx_sail & 63;
          if (c48_bitmap[idx] & (MSK >> off)) {
            ck_idx = c48_popcnt[idx] + POPCNT64(c48_bitmap[idx] >> (64 - off));
            idx_sail = (ck_idx << 8) + ((ip1 >> 8) & 0XFF);
            idx = idx_sail >> 6;
            off = idx_sail & 63;
            if (c56_bitmap[idx] & (MSK >> off)) {
              ck_idx = c56_popcnt[idx] + POPCNT64(c56_bitmap[idx] >> (64 - off));
              idx_sail = (ck_idx << 8) + (ip1 & 0XFF);
              idx = idx_sail >> 6;
              off = idx_sail & 63;
              if (c64_bitmap[idx] & (MSK >> off)) {
                ck_idx = c64_popcnt[idx] + POPCNT64(c64_bitmap[idx] >> (64 - off));
                idx_sail = (ck_idx << 8) + (ip2 >> 56);
                idx = idx_sail >> 6;
                off = idx_sail & 63;
                if (c72_bitmap[idx] & (MSK >> off)) {
                  ck_idx = c72_popcnt[idx] + POPCNT64(c72_bitmap[idx] >> (64 - off));
                  idx_sail = (ck_idx << 8) + ((ip2 >> 48) & 0XFF);
                  idx = idx_sail >> 6;
                  off = idx_sail & 63;
                  if (c80_bitmap[idx] & (MSK >> off)) {
                    ck_idx = c80_popcnt[idx] + POPCNT64(c80_bitmap[idx] >> (64 - off));
                    idx_sail = (ck_idx << 8) + ((ip2 >> 40) & 0XFF);
                    idx = idx_sail >> 6;
                    off = idx_sail & 63;
                    if (b88_bitmap[idx] & (MSK >> off)) {
                      n_idx = b88_popcnt[idx] + POPCNT64(b88_bitmap[idx] >> (64 - off));
                      return leafN[n_idx];
                    }
                  } else if (b80_bitmap[idx] & (MSK >> off)) {
                    n_idx = b80_popcnt[idx] + POPCNT64(b80_bitmap[idx] >> (64 - off));
                    return leafN[n_idx];
                  }
                } else if (b72_bitmap[idx] & (MSK >> off)) {
                  n_idx = b72_popcnt[idx] + POPCNT64(b72_bitmap[idx] >> (64 - off));
                  return leafN[n_idx];
                }
              } else if (b64_bitmap[idx] & (MSK >> off)) {
                n_idx = b64_popcnt[idx] + POPCNT64(b64_bitmap[idx] >> (64 - off));
                return leafN[n_idx];
              }
            } else if (b56_bitmap[idx] & (MSK >> off)) {
              n_idx = b56_popcnt[idx] + POPCNT64(b56_bitmap[idx] >> (64 - off));
              return leafN[n_idx];
            }
          } else if (b48_bitmap[idx] & (MSK >> off)) {
            n_idx = b48_popcnt[idx] + POPCNT64(b48_bitmap[idx] >> (64 - off));
            return leafN[n_idx];
          }
        } else if (b40_bitmap[idx] & (MSK >> off)) {
          n_idx = b40_popcnt[idx] + POPCNT64(b40_bitmap[idx] >> (64 - off));
          return leafN[n_idx];
        }
      } else if (b32_bitmap[idx] & (MSK >> off)) {
        n_idx = b32_popcnt[idx] + POPCNT64(b32_bitmap[idx] >> (64 - off));
        return leafN[n_idx];
      }
    } else if (b24_bitmap[idx] & (MSK >> off)) {
      n_idx = b24_popcnt[idx] + POPCNT64(b24_bitmap[idx] >> (64 - off));
      return leafN[n_idx];
    }
  } else if (b16_bitmap[idx] & (MSK >> off)) {
    n_idx = b16_popcnt[idx] + POPCNT64(b16_bitmap[idx] >> (64 - off));
    return leafN[n_idx];
  }
  return 1;
}
