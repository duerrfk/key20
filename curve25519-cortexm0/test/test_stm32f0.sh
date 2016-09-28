#!/bin/sh
DEVICE=/dev/ttyUSB0
DIR=`dirname $0`

stty -F $DEVICE raw icanon eof \^d 9600
st-flash write $DIR/test.bin 0x8000000
cat < $DEVICE | diff - $DIR/checksum
