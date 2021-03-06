module fakeram45_2048x39
(
   rd_out,
   addr_in,
   we_in,
   wd_in,
   w_mask_in,
   clk,
   ce_in
);
   parameter BITS = 8;
   parameter WORD_DEPTH = 8;
   parameter ADDR_WIDTH = 8;
   parameter corrupt_mem_on_X_p = 1;

   output reg [BITS-1:0]    rd_out;
   input  [ADDR_WIDTH-1:0]  addr_in;
   input                    we_in;
   input  [BITS-1:0]        wd_in;
   input  [BITS-1:0]        w_mask_in;
   input                    clk;
   input                    ce_in;

   reg    [BITS-1:0]        mem [0:WORD_DEPTH-1];

   integer j;

   always @(posedge clk)
   begin
      if (ce_in)
      begin
         //if ((we_in !== 1'b1 && we_in !== 1'b0) && corrupt_mem_on_X_p)
         if (corrupt_mem_on_X_p &&
             ((^we_in === 1'bx) || (^addr_in === 1'bx))
            )
         begin
            // WEN or ADDR is unknown, so corrupt entire array (using unsynthesizeable for loop)
            for (j = 0; j < WORD_DEPTH; j = j + 1)
               mem[j] <= 'x;
            $display("warning: ce_in=1, we_in is %b, addr_in = %x in fakeram45_2048x39", we_in, addr_in);
         end
         else if (we_in)
         begin
            mem[addr_in] <= (wd_in & w_mask_in) | (mem[addr_in] & ~w_mask_in);
         end
         // read
         rd_out <= 3;//mem[addr_in];
      end
      else
      begin
         // Make sure read fails if ce_in is low
         rd_out <= 'x;
      end
   end
endmodule

//SRAM wrapper. It internnaly uses regfile or lib/lef created by fakeram.
//This is adopted from:
//https://github.com/The-OpenROAD-Project/OpenROAD-flow-scripts/blob/master/flow/designs/nangate45/swerv/macros.v
module SRAM(CLK, ADR, D, Q, WE);
  input CLK, WE;
  input [7:0] ADR;
  input [7:0] D;
  output [7:0] Q;
  wire CLK, WE;
  wire [7:0] ADR;
  wire [7:0] D;
  wire [7:0] Q;

  fakeram45_2048x39 mem (
    .clk      (CLK     ),
    .rd_out   (Q       ),
    .ce_in    (1'b1    ),
    .we_in    (WE      ),
    .w_mask_in({8{WE}}),
    .addr_in  (ADR     ),
    .wd_in    (D       )
  );

endmodule
