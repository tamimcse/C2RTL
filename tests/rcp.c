/*
This code has been adapted from:
https://github.com/tamimcse/domino-examples/blob/master/domino-programs/rcp-real.c
*/

#include <stdint.h>

//Capacity of the line card in megabytes
#define C 64000 
#define T 50 //Control Interval in ms
#define A 50000 //1000*T

uint32_t rcp(uint32_t rtt, uint32_t tick, uint32_t queue, uint32_t size_bytes, uint32_t states[2]) {
  uint32_t R; //RCP feedback rate in MB/s
  uint32_t S; //Spare capacity in MB/s
  uint32_t RTT; // Running average of RTT in ms
  uint32_t B; //Number of Bytes received

  //retrieve the states
  RTT = states[0];
  B = states[1];

  //Calculate running average of RTT
  RTT = (RTT * 49 + rtt)/50;  

  //Control interval has expired, so
  // calculate the feeback throughput
  // and reset the state variables
  if (tick % T == 0) {
    S = C - B/A;
    B = 0;
    R *= 1+((S-((queue/RTT)/2))*T/RTT)/C;
  }
  else {
    B += size_bytes; 
  }

  //update the states
  states[0] = RTT;
  states[1] = B;

  return R;
}

