/*
Based on:
MD Iftakharul Islam et.al. "Leveraging Domino to Implement RCP in a Stateful Programmable Pipeline", IEEE HPSR 2019
*/

#include <stdint.h>

#define ALPHA (1)
#define BETA (1/2)

//states[0] = Running average of RTT in ms
//states[1] = Number of Bytes received
//states[2] = RCP feedback rate in MB/s
//states[3] = clock tick when the RCP rate has been calculated
//rtt: is the RTT of the packet
//tick: is the current time
//queue: current queue size
//packet_size: size of the packet in bytes
//control_interval: control interval in ms 
//capacity: capacity in megabytes
uint32_t rcp(uint32_t rtt, uint32_t tick, uint32_t queue, uint32_t packet_size, int control_interval, int capacity, uint32_t states[]) {
  uint32_t spare_capacity;
  
  if ((tick - states[3]) > control_interval) {
    spare_capacity = capacity - states[1] / (1000 * control_interval);
    states[0] = (states[0] * 49 + rtt)/50;
    states[1] = 0;
    states[2] *= 1 + ((control_interval/states[0]) * (ALPHA * spare_capacity - (BETA * queue)/states[0]))/capacity;
    states[3] = tick;
    return states[2];
  }
  else {
    states[0] = (states[0] * 49 + rtt)/50;
    states[1] += packet_size;
    return states[2];
  }
}

