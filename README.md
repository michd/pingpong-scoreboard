# Pingpong Scoreboard

This is an electronics hardware and firmware project for a Pingpong scoreboard.

Its user interface consists for a 4-digit 7 segment display, a few LEDs, a button for each side of the table, and a button for the scoreboard.

The scoreboard keeps track of the score within the game, within the set (games won each side since power-up) and of the overall score (games won each side, all time).

## Hardware

Since the program for this is really simple, and there isn't much I/O, an ATTiny84 was chose as the microcontroller, of the Microchip/AVR range. It is programmed via ISP, and I'm programming it using a Waveshare AVRISP MKII.

The display is driven using a Maxim Integrated MAX7219. The few extra indication LEDs are also driven by that, since the chip supports up to 8 digits, and the project only requires 4.

## Status

This is still a work in progress. The firmware does not implement all the intended features yet; the schematics and PCB(s) haven't yet been designed.
