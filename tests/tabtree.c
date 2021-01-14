#include <stdint.h>

uint8_t sail(uint32_t ip, uint8_t N16[100],
  uint16_t C16[100], uint8_t N24[100],
  uint16_t C24[100], uint8_t N32[100]) {

  unsigned int idx, cidx;

  idx = ip >> 16;
  cidx = C16[idx];
  if (cidx) {
    idx = (cidx << 8) + ((ip >> 8) & 255);
    cidx = C24[idx];
    if (cidx) {
      idx = (cidx << 8) + (ip  & 255);
      return N32[idx];
    } else {
      return N24[idx];
    }
  } else {
    return N16[idx];
  }
}

