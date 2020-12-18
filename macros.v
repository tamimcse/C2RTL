`include "fakeram45_2048x39.v"

//SRAM wrapper. It internnaly uses regfile or lib/lef created by fakeram.
//Simolar wrapper is also used in Swerv in OpenROAD
module SRAM(CLK, ADR, D, Q, WE);
  input CLK, WE;
  input [10:0] ADR;
  input [38:0] D;
  output [38:0] Q;
  wire CLK, WE;
  wire [10:0] ADR;
  wire [38:0] D;
  wire [38:0] Q;

  fakeram45_2048x39 mem (
    .clk      (CLK     ),
    .rd_out   (Q       ),
    .ce_in    (1'b1    ),
    .we_in    (WE      ),
    .w_mask_in({39{WE}}),
    .addr_in  (ADR     ),
    .wd_in    (D       )
  );

endmodule
