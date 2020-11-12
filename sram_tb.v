//https://esrd2014.blogspot.com/p/synchronous-static-ram.html#:~:text=Static%20Random%2DAccess%20Memory%20(SRAM,which%20must%20be%20periodically%20refreshed.

`timescale 1ns / 1ps
module syncRAM_tb;
 // Inputs
 reg [7:0] dataIn;
 reg [7:0] Addr;
 reg CS;
 reg WE;
 reg RD;
 reg Clk;

 // Outputs
 wire [7:0] dataOut;

 // Instantiate the Uniit Under Test (UUT)
 syncRAM uut (.dataIn(dataIn), .dataOut(dataOut), .Addr(Addr), .CS(CS), .WE(WE), .RD(RD), .Clk(Clk));

 initial begin
  $monitor("%d, Clk=%b, WE=%b, RD=%b, CS=%b Addr=%d, dataIn=%d,	dataOut=%d", $time, Clk, WE, RD, CS, Addr, dataIn, dataOut);
  // Initialize Inputs
  dataIn  = 8'h0;
  Addr  = 8'h0;
  CS  = 1'b0;
  WE  = 1'b0;
  RD  = 1'b0;
  Clk  = 1'b0;

  // Wait 100 ns for global reset to finish
  #100;

  // Add stimulus here
  dataIn  = 8'h0;
  Addr  = 8'h0;
  CS  = 1'b1;
  WE  = 1'b1;
  RD  = 1'b0;
  #20;
  dataIn  = 8'h0;
  Addr  = 8'h0;
  #20;
  dataIn  = 8'h1;
  Addr  = 8'h1;
  #20;
  dataIn  = 8'h10;
  Addr  = 8'h2;
  #20;
  dataIn  = 8'h6;
  Addr  = 8'h3;
  #20;
  dataIn  = 8'h12;
  Addr  = 8'h4;
  #40;
  Addr  = 8'h0;
  WE  = 1'b0;
  RD  = 1'b1;
  #20;
  Addr   = 8'h1;
  #20;
  Addr   = 8'h2;
  #20;
  Addr   = 8'h3;
  #20;
  Addr   = 8'h4;
  $finish;
 end

 always #10 Clk = ~Clk;

endmodule
