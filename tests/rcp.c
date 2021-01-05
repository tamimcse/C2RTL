/*
Based on:
MD Iftakharul Islam et.al. "Leveraging Domino to Implement RCP in a Stateful Programmable Pipeline", IEEE HPSR 2019
*/

#include <stdint.h>

#define ALPHA (1)
#define BETA (1/2)

//rtt_a: Running average of RTT in ms
//bytes_received: Number of Bytes received
//feedback_rate: RCP feedback rate in MB/s
//last_clock_tick = clock tick when the RCP rate has been calculated
//rtt: is the RTT of the packet
//tick: is the current time
//queue: current queue size
//packet_size: size of the packet in bytes
//control_interval: control interval in ms 
//capacity: capacity in megabytes
uint32_t rcp(uint32_t rtt, uint32_t tick, uint32_t queue, uint32_t packet_size, int control_interval, int capacity,
 uint32_t *rtt_a, uint32_t *bytes_received, uint32_t *feedback_rate, uint32_t *last_clock_tick) {
  uint32_t spare_capacity;
  
  if ((tick - *last_clock_tick) > control_interval) {
    spare_capacity = capacity - *bytes_received / (1000 * control_interval);
    *rtt_a = (*rtt_a * 49 + rtt)/50;
    *bytes_received = 0;
    *feedback_rate *= 1 + ((control_interval/(*rtt_a)) * (ALPHA * spare_capacity - (BETA * queue)/(*rtt_a)))/capacity;
    *last_clock_tick = tick;
    return *feedback_rate;
  }
  else {
    *rtt_a = (*rtt_a * 49 + rtt)/50;
    *bytes_received += packet_size;
    return *feedback_rate;
  }
}

