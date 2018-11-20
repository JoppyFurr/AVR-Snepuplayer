#!/bin/sh

# AVR-Snepuplayer
#
# This tool allows for Sega Master System music (converted from VGM format) to
# be played back on the Atmel ATMega8 microcontroller.
#
# The audio is output as PWM on pin 17. This should be amplified and low-pass
# filtered before being sent to a speaker.
#
# This script currently assumes an avr910-compatible programmer at /dev/ttyUSB0.
#
# Before first use, ensure the ATMega8 is configured to run at 8 MHz.
#
#   # The following configures the internal oscillator to 8 MHz
#   avrdude -p m8 -c avr910 -P /dev/ttyUSB0 -U lfuse:w:0xE4:m
#
# The script should be run with a VGM file as a parameter.

# Build the conversion tool
gcc -o vgm_convert vgm_convert.c -lz || exit

# Convert the VGM to something suitable for the micro
./vgm_convert "${1}" > music_data.inc || exit

# Assemble
avra ./snepuplayer.asm || exit

# Tidy up
rm snepuplayer.cof snepuplayer.eep.hex snepuplayer.obj

# Program
avrdude -p m8 -c avr910 -P /dev/ttyUSB0 -U flash:w:snepuplayer.hex

