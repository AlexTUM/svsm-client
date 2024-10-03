#! /bin/sh

echo 5 > /proc/sys/kernel/printk
cat /proc/1/maps
