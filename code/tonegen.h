#ifndef TONEGEN_H_
#define TONEGEN_H_

#include "stdbool.h"
#include "stdint.h"

#define TONEGEN_ON() TCCR1A |= 0x40;
#define TONEGEN_OFF() TCCR1A &= ~(0x40);

// These enums are provided for convenience and code readability.
// However, to conserve space, melodies will be written as sequences of
// 16 bit values, which can be written in hexadecimal as 4 digits.
// Digit 0 of this is the rightmost, the least significant 4 bits,
// and Digit 3 is the leftmost, most significant.
//
// Digit 0-1: This byte gives the duration in melody steps this note will be
//            held for
// Digit 2:   This nibble says what octave the note is to be played at
// Digit 3:   This nibble says which note is to be played, from toneGenNotes.

typedef enum { C, Db, D, Eb, E, F, Gb, G, Ab, A, Bb, B, Rest } tonegenNotes;

typedef enum {
  StartupMelo,
  WinMelo,
  ButtonPressSfx,
  ButtonLongPressSfx
} Melodies;

void tonegenInit();

void tonegenTriggerMelody(Melodies);

#endif // TONEGEN_H_
