#ifndef TONEGEN_H_
#define TONEGEN_H_

#include "stdbool.h"
#include "stdint.h"

typedef enum { C, Db, D, Eb, E, F, Gb, G, Ab, A, Bb, B } tonegenNotes;

/**
 * Turns sound output on or off.
 */
void tonegenToggle(bool);

/**
 * Plays a given note. Make sure to turn the tone generator on to actually
 * output it.
 * noteId is one from the tonegenNotes enum
 */
void tonegenPlayNote(tonegenNotes note, uint8_t octave);

#endif // TONEGEN_H_
