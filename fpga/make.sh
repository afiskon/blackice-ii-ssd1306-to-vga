#!/bin/sh

set -e 

./clean.sh

yosys -q -p "synth_ice40 -blif main.blif" main.sv
arachne-pnr -d 8k -P tq144:4k -p main.pcf main.blif -o main.asc
icetime -c 25 main.asc
icepack main.asc main.bin

# xxd -i main.bin | sed -e 's/unsigned/const unsigned/' > ../stm32/Inc/ice40_config_uncompressed.h

../compress/encode2 main.bin main.bin.enc2
xxd -i main.bin.enc2 | sed -e 's/unsigned/const unsigned/' > ../stm32/Inc/ice40_config_compressed.h
# stty -F /dev/ttyACM0 raw
# cat main.bin > /dev/ttyACM0
