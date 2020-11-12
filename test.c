#include <stdint.h>

uint8_t cptrie(uint8_t a, uint8_t b) { 
  uint8_t c;

  c = a + 5;
  if (c != 127) {
    c = c + 5;
    return c;
  } else {
    c = b + 5;
    return c*8;
  }
}

