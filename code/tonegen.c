#include "tonegen.h"
#include <avr/io.h>
static uint8_t BASE_OCTAVE = 4; 

// These are calculated based on:
// - 16MHz clock frequency
// - Timer prescaler of 8
// - Toggling pin on match
//
// Since the pin is toggled on every match, the frequency at which matches
// occur equals half the period of the output square wave.
// Thus: 16 000 000Hz / 8 / compare value = 2x the output frequency
// For example, A4: 16 000 000Hz / 8 / 227.7272 = 880 Hz = 2x 440Hz
static float compValues[] = {
  3822.1916, //C4  - 261.63Hz
  3607.7639, //Db4 - 277.18Hz
  3405.2986, //D4  - 293.66Hz
  3214.0906, //Eb4 - 311.13Hz
  3033.7045, //E4  - 329.63Hz
  2863.4424, //F4  - 349.23Hz
  2702.7758, //Gb4 - 369.99Hz
  2551.0204, //G4  - 392.00Hz
  2407.8979, //Ab4 - 415.30Hz
  2272.7272, //A4  - 440Hz
  2145.1862, //Bb4 - 466.16Hz
  2024.7833  //B4  - 493.88Hz
};

static uint16_t getCompValue(uint8_t noteIndex, uint8_t octave);

void tonegenToggle(bool on) {
  if (on) {
    // Set the timer pin to toggle on compare match
    TCCR1A |= 0x40;
  } else {
    // Disconnect the timer pin, nothing happens on it on compare match
    TCCR1A &= ~(0x40);
  }
}

void tonegenPlayNote(tonegenNotes note, uint8_t octave) {
  OCR1A = getCompValue((uint8_t)note, octave);
}

static uint16_t getCompValue(uint8_t noteIndex, uint8_t octave) {
  float multiplier = 1;
  int8_t octaveOffset = BASE_OCTAVE - octave;
  bool down = octaveOffset < 0;

  while (octaveOffset != 0) {
    multiplier *= down ? 0.5 : 2;
    octaveOffset += down ? 1 : -1;
  }

  if (noteIndex > 11) noteIndex = noteIndex % 12;

  float compValue = compValues[noteIndex] * multiplier;

  // Poor man's round()
  return (uint16_t)(compValue + 0.5);
}
