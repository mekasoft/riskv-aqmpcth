#!/bin/sh

# Put the program you want to run automatically here
# Execute this program at boot

/root/main.bin &
/root/dht22.bin &
/root/pms5003.bin &
/root/mhz19b.bin &