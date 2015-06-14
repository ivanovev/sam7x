#!/bin/bash

fname="build/ch.bin"
if [ -f $fname ]; then
    crc=`crc32 $fname`
    echo $crc
    tftp 192.168.0.1 -m binary -c put $fname $crc.bin
fi
