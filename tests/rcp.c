/*
This code has been adapted from:
https://github.com/tamimcse/domino-examples/blob/master/domino-programs/rcp-real.c
*/

#include <stdint.h>

//Capacity of the line card in megabytes
#define C 64000 
#define T 50 //Control Interval in ms
#define A 50000 //1000*T


//states[0] = Running average of RTT in ms
//states[1] = Number of Bytes received
//states[2] = RCP feedback rate in MB/s
uint32_t rcp(uint32_t rtt, uint32_t tick, uint32_t queue, uint32_t size_bytes, uint32_t states[2]) {
  uint32_t S; //Spare capacity in MB/s
  uint32_t RTT; // Running average of RTT in ms
  
  //Control interval has expired, so
  // calculate the feeback throughput
  // and reset the state variables
  if (tick % T == 0) {
    RTT = states[0];
    S = C - states[1]/A;
    states[1] = 0;
    states[2] *= 1+((S-((queue/RTT)/2))*T/RTT)/C;
    return states[2];
  }
  else {
    //Calculate running average of RTT
    states[0] = (states[0] * 49 + rtt)/50;
    //Update the number of Bytes received
    states[1] += size_bytes;
    return states[2];
  }
}

