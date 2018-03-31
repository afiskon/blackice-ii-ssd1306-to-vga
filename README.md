# blackice-ii-ssd1306-to-vga

BlackIce II: example of STM32 &lt;-> ICE40 communication.

STM32 MCU stores ICE40 FPGA configuration in internal flash memory. After
restart the MCU configures the FPGA (see [iCE40 Programming and
Configuration][u1] document). Then the MCU communicates with the configured FPGA
over SPI bus using a protocol similar to the one used by SSD1306-based OLED
LCDs. ICE40 receives the data and draws a corresponding image on a VGA display.

To run this example the following hardware is required:

* BlackIce II development board;
* STLink v2 programmer;
* Digilent PmodVGA module (plug it into PMOD9/10 and PMOD11/12 sockets of
  BlackIce II).

See also:

* https://github.com/afiskon/stm32-ssd1306
* https://github.com/afiskon/fpga-ssd1306-to-vga

[u1]: http://www.latticesemi.com/-/media/LatticeSemi/Documents/ApplicationNotes/IK/iCE40ProgrammingandConfiguration.ashx
