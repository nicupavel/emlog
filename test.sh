#!/bin/bash
echo ---- making emlog and nbcat
make emlog.ko
make nbcat
echo ---- inserting module
insmod emlog.ko
echo ---- testing module
mknod testlog c 250 8
echo "testing testlog."$RANDOM > testlog
./nbcat testlog
rm testlog
echo ---- removing module
rmmod emlog
echo ---- dmesg out
dmesg | tail
