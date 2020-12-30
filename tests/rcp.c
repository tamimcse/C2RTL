/*
This code has been adapted from:
https://github.com/tamimcse/domino-examples/blob/master/domino-programs/rcp-real.c
*/

//Capacity of the line card in megabytes
#define C 64000 
#define T 50 //Control Interval in ms
#define A 50000 //1000*T

int rcp(int rtt, int tick, int queue, int size_bytes, int states[2]) {
  int R; //RCP feedback rate in MB/s
  int S; //Spare capacity in MB/s
  int RTT; // Running average of RTT in ms
  int B; //Number of Bytes received

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

