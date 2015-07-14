#!/bin/bash

#ipaddr="192.168.0.1"
ipaddr="192.168.4.217"
fname="build/ch.bin"
if [ -f $fname ]; then
    crc=`crc32 $fname`
    echo $crc
    tftp $ipaddr -m binary -c put $fname $crc.bin
fi
