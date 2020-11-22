#include <stdint.h>

#define CNK24 60
#define CNK32 6000
#define CNK40 7000
#define CNK48 11000
#define CNK56 700
#define CNK64 700
#define CNK72 500
#define CNK80 500
#define CNK88 500
#define CNK96 500
#define CNK104 500
#define CNK112 500
#define CNK120 500
#define CNK128 500

/*chunk size is 2^8*/
#define CNK_8 256

#define SIZE16 65536
#define SIZE24 CNK24 * CNK_8
#define SIZE32 CNK32 * CNK_8
#define SIZE40 CNK40 * CNK_8
#define SIZE48 CNK48 * CNK_8
#define SIZE56 CNK56 * CNK_8
#define SIZE64 CNK64 * CNK_8

#define SIZE72 CNK72 * CNK_8
#define SIZE80 CNK80 * CNK_8
#define SIZE88 CNK88 * CNK_8
#define SIZE96 CNK96 * CNK_8
#define SIZE104 CNK104 * CNK_8
#define SIZE112 CNK112 * CNK_8
#define SIZE120 CNK120 * CNK_8
#define SIZE128 CNK128 * CNK_8

// SAIL based FIB lookup
int8_t sail (uint8_t N16[SIZE16], uint16_t C16[SIZE16], uint8_t N24[SIZE24], uint32_t C24[SIZE24],
  uint8_t N32[SIZE32], uint32_t C32[SIZE32], uint8_t N40[SIZE40], uint32_t C40[SIZE40], 
  uint8_t N48[SIZE48], uint32_t C48[SIZE48], uint8_t N56[SIZE56], uint32_t C56[SIZE56],
  uint8_t N64[SIZE64], uint32_t C64[SIZE64], uint8_t N72[SIZE72], uint32_t C72[SIZE72],
  uint8_t N80[SIZE80], uint32_t C80[SIZE80], uint8_t N88[SIZE88], uint32_t C88[SIZE88],
  uint8_t N96[SIZE96], uint32_t C96[SIZE96], uint8_t N104[SIZE104], uint32_t C104[SIZE104],
  uint8_t N112[SIZE112], uint32_t C112[SIZE112], uint8_t N120[SIZE120], uint32_t C120[SIZE120],
  uint8_t N128[SIZE128],
  uint64_t ip1, uint64_t ip2)
{
  int i,j,k;
  uint32_t idx = ip1 >> 48;
  if (C16[idx]) {
    idx = ((C16[idx] - 1) << 8) + ((ip1 >> 40) & 0XFF);
    if (C24[idx]) {
      idx = ((C24[idx] - 1) << 8) + ((ip1 >> 32) & 0XFF);
      if (C32[idx]) {
        idx = ((C32[idx] - 1) << 8) + ((ip1 >> 24) & 0XFF);
        if (C40[idx]) {
          idx = ((C40[idx] - 1) << 8) + ((ip1 >> 16) & 0XFF);
          if (C48[idx]) {
            idx = ((C48[idx] - 1) << 8) + ((ip1 >> 8) & 0XFF);
            if (C56[idx]) {
              idx = ((C56[idx] - 1) << 8) + (ip1 & 0XFF);
              if (C64[idx]) {
                idx = ((C64[idx] - 1) << 8) + (ip2 >> 56);
                if (C72[idx]) {
                  idx = ((C72[idx] - 1) << 8) + ((ip2 >> 48) & 0XFF);
                  if (C80[idx]) {
                    idx = ((C80[idx] - 1) << 8) + ((ip2 >> 40) & 0XFF);
                    if (C88[idx]) {
                      idx = ((C88[idx] - 1) << 8) + ((ip2 >> 32) & 0XFF);
                      if (C96[idx]) {
                        idx = ((C96[idx] - 1) << 8) + ((ip2 >> 24) & 0XFF);
                        if (C104[idx]) {
                          idx = ((C104[idx] - 1) << 8) + ((ip2 >> 16) & 0XFF);
                          if (C112[idx]) {
                            idx = ((C112[idx] - 1) << 8) + ((ip2 >> 8) & 0XFF);
                            if (C120[idx]) {
                              idx = ((C120[idx] - 1) << 8) + (ip2 & 0XFF);
                              return N128[idx];
                            } else {
                              return N120[idx];
                            }
                          } else {
                            return N112[idx];
                          }
                        } else {
                          return N104[idx];
                        }
                      } else {
                        return N96[idx];
                      }
                    } else {
                      return N88[idx];
                    }
                  } else {
                    return N80[idx];
                  }
                } else {
                  return N72[idx];
                }
              } else {
                return N64[idx];
              }
            } else {
              return N56[idx];
            }
          } else {
            return N48[idx];
          }
        } else {
          return N40[idx];
        }
      } else {
        return N32[idx];
      }
    } else {
      return N24[idx];
    }
  } else {
    return N16[idx];
  }
}