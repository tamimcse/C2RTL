/*
This code has been take from Domino project. This has been adopted from:
https://github.com/packet-transactions/domino-examples/blob/master/domino_programs/heavy_hitters.c
*/
#include "hashes.h"
#include <stdint.h>
#include <stdbool.h>


#define NUM_ENTRIES 65536

bool count_min_sketch(uint64_t mac_addr, uint32_t hash1[NUM_ENTRIES],
     uint32_t hash2[NUM_ENTRIES], uint32_t hash3[NUM_ENTRIES]) {
  int hash1_idx;
  int hash2_idx;
  int hash3_idx;

  hash1 = hash2a(sport, dport) % NUM_ENTRIES;
  hash2 = hash2b(sport, dport) % NUM_ENTRIES;
  hash3 = hash2c(sport, dport) % NUM_ENTRIES;

  sketch_cnt_1[sketch1_idx]+= 1;
  sketch_cnt_2[sketch2_idx]+= 1;
  sketch_cnt_3[sketch3_idx]+= 1;

  if (sketch_cnt_1[sketch1_idx] > low_th && sketch_cnt_1[sketch1_idx] < hi_th &&
	    sketch_cnt_2[sketch2_idx] > low_th && sketch_cnt_2[sketch2_idx] < hi_th &&
	    sketch_cnt_3[sketch3_idx] > low_th && sketch_cnt_3[sketch3_idx] < hi_th) {
		return false;
  }	else {
		return true;
  }
}
