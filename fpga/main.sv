/* vim: set ai et ts=4 sw=4: */

`default_nettype none

module top(
    // 100MHz clock input
    input  CLK100MHz,
    // Global internal reset connected to RTS on ch340 and also PMOD[1]
    input greset,
    // Input lines from STM32/Done can be used to signal to Ice40 logic
    input DONE, // could be used as interupt in post programming
    input DBG1, // Could be used to select coms via STM32 or RPi etc..

    // SRAM Memory lines
    //output [17:0] ADR,
    //output [15:0] DAT,
    //output RAMOE,
    //output RAMWE,
    //output RAMCS,
    //output RAMLB,
    //output RAMUB,

    input logic wclk,
    input logic din,
    input logic cs,
    // input logic dc,

    output logic ld3,
    output logic [3:0] vga_r,
    output logic [3:0] vga_g,
    output logic [3:0] vga_b,
    output logic vga_hsync, // PMOD[36]
    output logic vga_vsync, // PMOD[37]

    input B1,
    input B2
);

logic clk;

// 640x480 @ 60Hz
// 25 Mhz, see `icepll -i 100 -o 25`
SB_PLL40_CORE #(
    .FEEDBACK_PATH("SIMPLE"),
    .PLLOUT_SELECT("GENCLK"),
    .DIVR(4'b0000),
    .DIVF(7'b0000111),
    .DIVQ(3'b101),
    .FILTER_RANGE(3'b001),
) uut (
    .REFERENCECLK(CLK100MHz),
    .PLLOUTCORE(clk),
    .LOCK(ld3), // LD3 (MISO not used anyway)
    .RESETB(1'b1),
    .BYPASS(1'b0)
);

parameter addr_width = 13;
logic mem [(1<<addr_width)-1:0];

logic [addr_width-1:0] raddr = 0;
logic [addr_width-1:0] waddr = 0;

always_ff @(posedge wclk) // write memory
begin
    if(cs == 0) // chip select
    begin
        //if(dc) // dc = high, accept data
        //begin
          mem[waddr] <= din;
          waddr <= waddr + 1;
        //end // dc = low, ignore command
    end
    else
        waddr <= 0;
end

parameter h_pulse  = 96;  // h-sunc pulse width
parameter h_bp     = 48;  // back porch pulse width
parameter h_pixels = 640; // number of pixels horizontally
parameter h_fp     = 16;  // front porch pulse width
parameter h_frame  = h_pulse + h_bp + h_pixels + h_fp;

parameter v_pulse  = 2;   // v-sync pulse width
parameter v_bp     = 31;  // back porch pulse width
parameter v_pixels = 480; // number of pixels vertically
parameter v_fp     = 11;  // front porch pulse width
parameter v_frame  = v_pulse + v_bp + v_pixels + v_fp;

parameter border   = 10;
parameter h_offset = h_pulse + h_bp + ((h_pixels - (128*4))/2);
parameter v_offset = v_pulse + v_bp + ((v_pixels - (64*4))/2);

logic [addr_width-1:0] h_pos = 0;
logic [addr_width-1:0] v_pos = 0;

logic [addr_width-1:0] x = 0;
logic [addr_width-1:0] y = 0;

logic [3:0] color = 4'b0000;
assign vga_r[3:0] = color;
assign vga_g[3:0] = color;
assign vga_b[3:0] = color;

assign vga_hsync = (h_pos < h_pulse) ? 0 : 1;
assign vga_vsync = (v_pos < v_pulse) ? 0 : 1;

// Required for consistency between all registers since `<=` is asynchonous.
// The value 22 was found experimentally. This is a dirty hack, I know :(
parameter MAGIC = 22; 

always_ff @(posedge clk) begin
    // update current position
    if(h_pos < h_frame - 1)
    begin
        h_pos <= h_pos + 1;
        if(h_pos <= (h_offset - MAGIC))
            x <= 0;
        else
            x <= x + 1;
    end
    else
    begin
        h_pos <= 0;
        x <= 0;
        if(v_pos < v_frame - 1)
        begin
            v_pos <= v_pos + 1;
            if(v_pos <= v_offset)
                y <= 0;
            else
                y <= y + 1;
        end
        else
        begin
            v_pos <= 0;
            y <= 0;
        end
    end

    // addr = (X + (Y / 8) * 128)*8 + (7 - Y % 8)
    // X = x / 4
    // Y = y / 4
    raddr <= ((x / 4) + ((y/4)/8)*128)*8 + (7 - (y/4) & 13'b111);

    // are we inside centered 512x256 area plus border?
    if((h_pos >= h_offset - border) &&
       (h_pos < (h_offset + (128*4) + border)) &&
       (v_pos >= v_offset - border) &&
       (v_pos < (v_offset + (64*4) + border)))
    begin
        if((h_pos >= h_offset) && (h_pos < h_offset + (128*4)) &&
           (v_pos >= v_offset) && (v_pos < v_offset + (64*4)))
        begin // inside centered area
            if(mem[raddr])
                color <= 4'b1111;
            else
                color <= 4'b0000;
        end
        else // outside centered area, draw the border
            color <= 4'b1111;
    end
    else // everything else is black
        color <= 4'b0000;
end

endmodule
