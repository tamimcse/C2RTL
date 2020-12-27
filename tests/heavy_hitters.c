/*
This code has been take from Domino project. This has been adopted from:
https://github.com/packet-transactions/domino-examples/blob/master/domino_programs/heavy_hitters.c
*/
#include "hashes.h"

#define low_th 100
#define hi_th  1000

#define NUM_ENTRIES 4096

int sketch_cnt_1[NUM_ENTRIES] = {0};
int sketch_cnt_2[NUM_ENTRIES] = {0};
int sketch_cnt_3[NUM_ENTRIES] = {0};

void count_min_sketch(int sport, int dport) {
  int sketch1_idx;
  int sketch2_idx;
  int sketch3_idx;

  sketch1_idx = hash2a(sport, dport) % NUM_ENTRIES;
  sketch2_idx = hash2b(sport, dport) % NUM_ENTRIES;
  sketch3_idx = hash2c(sport, dport) % NUM_ENTRIES;

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
