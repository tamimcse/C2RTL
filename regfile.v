//https://esrd2014.blogspot.com/p/synchronous-static-ram.html#:~:text=Static%20Random%2DAccess%20Memory%20(SRAM,which%20must%20be%20periodically%20refreshed.
module regfile( dataIn, dataOut, Addr, CS, WE, RD, Clk);
parameter ADR   = 8;
parameter DAT   = 8;
parameter DPTH  = 8;

//ports
input   [DAT-1:0]  dataIn;
output reg [DAT-1:0]  dataOut;
input   [ADR-1:0]  Addr;
input CS, WE, RD, Clk;

//internal variables
reg [DAT-1:0] SRAM [DPTH-1:0];

always @ (posedge Clk)
begin
 if (CS == 1'b1) begin
  if (WE == 1'b1 && RD == 1'b0) begin
   SRAM [Addr] = dataIn;
  end
  else if (RD == 1'b1 && WE == 1'b0) begin
   dataOut = 3;//SRAM [Addr]; 
  end
  else;
 end
 else;
end
endmodule
