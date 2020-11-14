#include <stdint.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

#define DIRSIZE 65536
#define SIZE16 400
#define SIZE22 3000
#define SIZE28 15000
#define SIZE34 14000
#define SIZE40 15000
#define SIZE46 21000
#define SIZE52 2000
#define SIZE58 2000
#define SIZE64 1000
#define SIZE70 100
#define SIZE76 100
#define SIZE82 100
#define SIZE88 100
#define SIZE94 100
#define SIZE100 100
#define SIZE106 100
#define SIZE112 100
#define SIZE118 100
#define SIZE124 100
#define N_SIZE 1500000

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

// Poptrie based FIB lookup
int8_t poptrie (int8_t N16[SIZE16], int16_t dirC[DIRSIZE], 
  uint64_t vec16[SIZE16], uint64_t leafvec16[SIZE16], uint32_t base0_16[SIZE16], uint32_t base1_16[SIZE16],
  uint64_t vec22[SIZE22], uint64_t leafvec22[SIZE22], uint32_t base0_22[SIZE22], uint32_t base1_22[SIZE22],
  uint64_t vec28[SIZE28], uint64_t leafvec28[SIZE28], uint32_t base0_28[SIZE28], uint32_t base1_28[SIZE28],
  uint64_t vec34[SIZE34], uint64_t leafvec34[SIZE34], uint32_t base0_34[SIZE34], uint32_t base1_34[SIZE34],
  uint64_t vec40[SIZE40], uint64_t leafvec40[SIZE40], uint32_t base0_40[SIZE40], uint32_t base1_40[SIZE40],
  uint64_t vec46[SIZE46], uint64_t leafvec46[SIZE46], uint32_t base0_46[SIZE46], uint32_t base1_46[SIZE46],
  uint64_t vec52[SIZE52], uint64_t leafvec52[SIZE52], uint32_t base0_52[SIZE52], uint32_t base1_52[SIZE52],
  uint64_t vec58[SIZE58], uint64_t leafvec58[SIZE58], uint32_t base0_58[SIZE58], uint32_t base1_58[SIZE58],
  uint64_t vec64[SIZE64], uint64_t leafvec64[SIZE64], uint32_t base0_64[SIZE64], uint32_t base1_64[SIZE64],
  uint64_t vec70[SIZE70], uint64_t leafvec70[SIZE70], uint32_t base0_70[SIZE70], uint32_t base1_70[SIZE70],
  uint64_t vec76[SIZE76], uint64_t leafvec76[SIZE76], uint32_t base0_76[SIZE76], uint32_t base1_76[SIZE76],
  uint64_t vec82[SIZE82], uint64_t leafvec82[SIZE82], uint32_t base0_82[SIZE82], uint32_t base1_82[SIZE82],
  uint64_t vec88[SIZE88], uint64_t leafvec88[SIZE88], uint32_t base0_88[SIZE88], uint32_t base1_88[SIZE88],
  uint64_t vec94[SIZE94], uint64_t leafvec94[SIZE94], uint32_t base0_94[SIZE94], uint32_t base1_94[SIZE94],
  uint64_t vec100[SIZE100], uint64_t leafvec100[SIZE100], uint32_t base0_100[SIZE100], uint32_t base1_100[SIZE100],
  uint64_t vec106[SIZE106], uint64_t leafvec106[SIZE106], uint32_t base0_106[SIZE106], uint32_t base1_106[SIZE106],
  uint64_t vec112[SIZE112], uint64_t leafvec112[SIZE112], uint32_t base0_112[SIZE112], uint32_t base1_112[SIZE112],
  uint64_t vec118[SIZE118], uint64_t leafvec118[SIZE118], uint32_t base0_118[SIZE118], uint32_t base1_118[SIZE118],
  uint64_t vec124[SIZE124], uint64_t leafvec124[SIZE124], uint32_t base0_124[SIZE124], uint32_t base1_124[SIZE124],
  int8_t leafN[N_SIZE], int64_t ip1, int64_t ip2)
{
  int i,j,k;
  uint32_t idx = ip1 >> 48;
  uint32_t off;
  uint32_t n_idx;

  if (dirC[idx]) {
    idx = dirC[idx] - 1;
    off = (ip1 >> 42) & 63;
    if (vec16[idx] & (1ULL << off)) {
      idx = base0_16[idx] + POPCNT64 (vec16[idx] & ((2ULL << off) - 1)) - 1;
      off = (ip1 >> 36) & 63;
      if (vec22[idx] & (1ULL << off)) {
        idx = base0_22[idx] + POPCNT64 (vec22[idx] & ((2ULL << off) - 1)) - 1;
        off = ((ip1 >> 30) & 63);
        if (vec28[idx] & (1ULL << off)) {
          idx = base0_28[idx] + POPCNT64 (vec28[idx] & ((2ULL << off) - 1)) - 1;
          off = ((ip1 >> 24) & 63);
          if (vec34[idx] & (1ULL << off)) {
            idx = base0_34[idx] + POPCNT64 (vec34[idx] & ((2ULL << off) - 1)) - 1;
            off = ((ip1 >> 18) & 63);
            if (vec40[idx] & (1ULL << off)) {
              idx = base0_40[idx] + POPCNT64 (vec40[idx] & ((2ULL << off) - 1)) - 1;
              off = ((ip1 >> 12) & 63);
              if (vec46[idx] & (1ULL << off)) {
                idx = base0_46[idx] + POPCNT64 (vec46[idx] & ((2ULL << off) - 1)) - 1;
                off = ((ip1 >> 6) & 63);
                if (vec52[idx] & (1ULL << off)) {
                  idx = base0_52[idx] + POPCNT64 (vec52[idx] & ((2ULL << off) - 1)) - 1;
                  off = ip1 & 63;
                  if (vec58[idx] & (1ULL << off)) {
                    idx = base0_58[idx] + POPCNT64 (vec58[idx] & ((2ULL << off) - 1)) - 1;
                    off = ip2 >> 58;
                    if (vec64[idx] & (1ULL << off)) {
                      idx = base0_64[idx] + POPCNT64 (vec64[idx] & ((2ULL << off) - 1)) - 1;
                      off = ((ip2 >> 52) & 63);
                      if (vec70[idx] & (1ULL << off)) {
                        idx = base0_70[idx] + POPCNT64 (vec70[idx] & ((2ULL << off) - 1)) - 1;
                        off = ((ip2 >> 46) & 63);
                        if (vec76[idx] & (1ULL << off)) {
                          idx = base0_76[idx] + POPCNT64 (vec76[idx] & ((2ULL << off) - 1)) - 1;
                          off = ((ip2 >> 40) & 63);
                          if (vec82[idx] & (1ULL << off)) {
                            idx = base0_82[idx] + POPCNT64 (vec82[idx] & ((2ULL << off) - 1)) - 1;
                            off = ((ip2 >> 34) & 63);
                            if (vec88[idx] & (1ULL << off)) {
                              idx = base0_88[idx] + POPCNT64 (vec88[idx] & ((2ULL << off) - 1)) - 1;
                              off = ((ip2 >> 28) & 63);
                              if (vec94[idx] & (1ULL << off)) {
                                idx = base0_94[idx] + POPCNT64 (vec94[idx] & ((2ULL << off) - 1)) - 1;
                                off = ((ip2 >> 22) & 63);
                                if (vec100[idx] & (1ULL << off)) {
                                  idx = base0_100[idx] + POPCNT64 (vec100[idx] & ((2ULL << off) - 1)) - 1;
                                  off = ((ip2 >> 16) & 63);
                                  if (vec106[idx] & (1ULL << off)) {
                                    idx = base0_106[idx] + POPCNT64 (vec106[idx] & ((2ULL << off) - 1)) - 1;
                                    off = ((ip2 >> 10) & 63);
                                    if (vec112[idx] & (1ULL << off)) {
                                      idx = base0_112[idx] + POPCNT64 (vec112[idx] & ((2ULL << off) - 1)) - 1;
                                      off = ((ip2 >> 4) & 63);
                                      if (vec118[idx] & (1ULL << off)) {
                                        idx = base0_118[idx] + POPCNT64 (vec118[idx] & ((2ULL << off) - 1)) - 1;
                                        off = ((ip2 << 2) & 63);
                                        //Reached the leaf
                                        if (leafvec124[idx] & (1ULL << off)) {
                                          n_idx = base1_124[idx] + POPCNT64 (leafvec124[idx] & ((2ULL << off) - 1)) - 1;
                                          return leafN[n_idx];
                                        }
                                      } else if (leafvec118[idx] & (1ULL << off)) {
                                        n_idx = base1_118[idx] + POPCNT64 (leafvec118[idx] & ((2ULL << off) - 1)) - 1;
                                        return leafN[n_idx];
                                      }
                                    } else if (leafvec112[idx] & (1ULL << off)) {
                                      n_idx = base1_112[idx] + POPCNT64 (leafvec112[idx] & ((2ULL << off) - 1)) - 1;
                                      return leafN[n_idx];
                                    }
                                  } else if (leafvec106[idx] & (1ULL << off)) {
                                    n_idx = base1_106[idx] + POPCNT64 (leafvec106[idx] & ((2ULL << off) - 1)) - 1;
                                    return leafN[n_idx];
                                  }
                                } else if (leafvec100[idx] & (1ULL << off)) {
                                  n_idx = base1_100[idx] + POPCNT64 (leafvec100[idx] & ((2ULL << off) - 1)) - 1;
                                  return leafN[n_idx];
                                }
                              } else if (leafvec94[idx] & (1ULL << off)) {
                                n_idx = base1_94[idx] + POPCNT64 (leafvec94[idx] & ((2ULL << off) - 1)) - 1;
                                return leafN[n_idx];
                              }
                            } else if (leafvec88[idx] & (1ULL << off)) {
                              n_idx = base1_88[idx] + POPCNT64 (leafvec88[idx] & ((2ULL << off) - 1)) - 1;
                              return leafN[n_idx];
                            }
                          } else if (leafvec82[idx] & (1ULL << off)) {
                            n_idx = base1_82[idx] + POPCNT64 (leafvec82[idx] & ((2ULL << off) - 1)) - 1;
                            return leafN[n_idx];
                          }
                        } else if (leafvec76[idx] & (1ULL << off)) {
                          n_idx = base1_76[idx] + POPCNT64 (leafvec76[idx] & ((2ULL << off) - 1)) - 1;
                          return leafN[n_idx];
                        }
                      } else if (leafvec70[idx] & (1ULL << off)) {
                        n_idx = base1_70[idx] + POPCNT64 (leafvec70[idx] & ((2ULL << off) - 1)) - 1;
                        return leafN[n_idx];
                      }
                    } else if (leafvec64[idx] & (1ULL << off)) {
                      n_idx = base1_64[idx] + POPCNT64 (leafvec64[idx] & ((2ULL << off) - 1)) - 1;
                      return leafN[n_idx];
                    }
                  } else if (leafvec58[idx] & (1ULL << off)) {
                    n_idx = base1_58[idx] + POPCNT64 (leafvec58[idx] & ((2ULL << off) - 1)) - 1;
                    return leafN[n_idx];
                  }
                } else if (leafvec52[idx] & (1ULL << off)) {
                  n_idx = base1_52[idx] + POPCNT64 (leafvec52[idx] & ((2ULL << off) - 1)) - 1;
                  return leafN[n_idx];
                }
              } else if (leafvec46[idx] & (1ULL << off)) {
                n_idx = base1_46[idx] + POPCNT64 (leafvec46[idx] & ((2ULL << off) - 1)) - 1;
                return leafN[n_idx];
              }
            } else if (leafvec40[idx] & (1ULL << off)) {
              n_idx = base1_40[idx] + POPCNT64 (leafvec40[idx] & ((2ULL << off) - 1)) - 1;
              return leafN[n_idx];
            }
          } else if (leafvec34[idx] & (1ULL << off)) {
            n_idx = base1_34[idx] + POPCNT64 (leafvec34[idx] & ((2ULL << off) - 1)) - 1;
            return leafN[n_idx];
          }
        } else if (leafvec28[idx] & (1ULL << off)) {
          n_idx = base1_28[idx] + POPCNT64 (leafvec28[idx] & ((2ULL << off) - 1)) - 1;
          return leafN[n_idx];
        }
      } else if (leafvec22[idx] & (1ULL << off)) {
        n_idx = base1_22[idx] + POPCNT64 (leafvec22[idx] & ((2ULL << off) - 1)) - 1;
        return leafN[n_idx];
      }
    } else if (leafvec16[idx] & (1ULL << off)) {
      n_idx = base1_16[idx] + POPCNT64 (leafvec16[idx] & ((2ULL << off) - 1)) - 1;
      return leafN[n_idx];
    }
    return 1;
  } else {
    return N16[idx];
  }
}
