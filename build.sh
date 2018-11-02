#!/bin/sh

# Assemble
avra ./snepuplayer.asm || exit

# Tidy up
rm snepuplayer.cof snepuplayer.eep.hex snepuplayer.obj

# Program

avrdude -p m8 -c avr910 -P /dev/ttyUSB0 -U flash:w:snepuplayer.hex

# Select 8 MHz Internal RC Oscillator
# avrdude -p m8 -c avr910 -P /dev/ttyUSB0 -U lfuse:w:0xE4:m
