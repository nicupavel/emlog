#!/bin/bash
echo ---- making emlog and nbcat
make
make nbcat
echo ---- inserting module
insmod emlog.ko
cat /proc/devices | grep emlog
ls -la -tr /sys/class/emlog
echo ---- testing module
mknod testlog c 250 8
echo "testing testlog."$RANDOM > testlog
./nbcat testlog
rm testlog
echo ---- removing module
rmmod emlog
echo ---- dmesg out
dmesg | tail
