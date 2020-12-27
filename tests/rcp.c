/*
This code has been adapted from:
https://github.com/tamimcse/domino-examples/blob/master/domino-programs/rcp-real.c
*/

//Capacity of the line card in megabytes
#define C 64000 
#define T 50 //Control Interval in ms
#define A 50000 //1000*T

/*********State Variables****************/
// Running average of RTT in ms
int RTT = 200; 
int R = 200; //RCP feedback rate in MB/s
int B = 0; //Number of Bytes received
int S = 0; //Spare capacity in MB/s

int rcp(int rtt, int tick, int queue, int size_bytes, int feedback_rate) {
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

  //if current feedback rate is smaller, don't update it
  return (feedback_rate > R)? R : -1;
}

