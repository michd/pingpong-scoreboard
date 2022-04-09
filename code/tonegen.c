#include "tonegen.h"
#include "animation.h"
#include <avr/io.h>
#include "stddef.h"

static uint16_t startupSeq[] = {
  0x0401, // C4, 1
  0x4401, // E4, 1
  0x7401, // G4, 1
  0x0502  // C5, 2
};

static uint16_t winSeq[] = {
  0x7401, // G4, 1
  0x0501, // C5, 1
  0x4501, // E5, 1
  0x7501, // G5, 1
  0xC401, // R,  1
  0x4501, // E5, 1
  0x7504  // G5, 4
};

static uint16_t buttonPressSeq[] = {
  0x0401, // C4, 1
  0x0501, // C5, 1
};

static uint16_t buttonLongPressSeq[] = {
  0x0503, // G5, 3
  0x0402, // C4, 1
};

typedef struct {
  uint16_t * seqPtr;
  uint16_t length;
  // differs in that this is how many ticks it takes to complete
  uint16_t duration; 
  uint16_t stepTicks;
  uint16_t position;
  uint16_t stepPosition;
} Melody;

static uint8_t BASE_OCTAVE = 4; 
static volatile Animation melodyAnim;

static Melody startupMelody;
static Melody winMelody;
static Melody buttonPressMelody;
static Melody buttonLongPressMelody;

static volatile Melody * activeMelody;

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
static void calcMelodyDuration(Melody * );
static void decodeStep(
    uint16_t raw,
    uint8_t * outNnote, uint8_t * outOctave, uint8_t * outDuration);
static void playNote(tonegenNotes note, uint8_t octave);
static void melodyFrame(Animation *);

void tonegenInit() {
  startupMelody.seqPtr = startupSeq;
  startupMelody.length = sizeof(startupSeq) / sizeof(uint16_t);
  startupMelody.stepTicks = 50;
  calcMelodyDuration(&startupMelody);

  buttonPressMelody.seqPtr = buttonPressSeq;
  buttonPressMelody.length = sizeof(buttonPressSeq) / sizeof(uint16_t);
  buttonPressMelody.stepTicks = 25;
  calcMelodyDuration(&buttonPressMelody);

  buttonLongPressMelody.seqPtr = buttonLongPressSeq;
  buttonLongPressMelody.length = sizeof(buttonLongPressSeq) / sizeof(uint16_t);
  buttonLongPressMelody.stepTicks = 25;
  calcMelodyDuration(&buttonLongPressMelody);

  winMelody.seqPtr = winSeq;
  winMelody.length = sizeof(winSeq) / sizeof(uint16_t);
  winMelody.stepTicks = 75;
  calcMelodyDuration(&winMelody);

  melodyAnim.frame = (animatorFunction)melodyFrame;
}

void tonegenTriggerMelody(Melodies melodyName) {
  Melody * melo;

  if (melodyName == ButtonPressSfx && activeMelody != NULL 
      && activeMelody != &buttonPressMelody
      && activeMelody != &buttonLongPressMelody) {
    // If we have a melody playing, don't interrupt it with the button press
    // sound effect
    return;
  }

  TONEGEN_OFF();
  animationSetActiveMelody(NULL);

  switch (melodyName) {
    case StartupMelo: melo = &startupMelody; break;
    case WinMelo: melo = &winMelody; break;
    case ButtonPressSfx: melo = &buttonPressMelody; break;
    case ButtonLongPressSfx: melo = &buttonLongPressMelody; break;
    default: return;
  }

  melo->position = 0;
  melo->stepPosition = 0;
  activeMelody = melo;

  melodyAnim.stepTicks = melo->stepTicks;
  melodyAnim.position = 0;
  melodyAnim.duration = melo->duration;
  animationSetActiveMelody(&melodyAnim);
}

void tonegenClear() {
  activeMelody = NULL;
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

static void decodeStep(
    uint16_t raw,
    uint8_t * outNote, uint8_t * outOctave, uint8_t * outDuration) {
  *outNote = (uint8_t)((raw & 0xF000) >> 12);
  *outOctave = (uint8_t)((raw & 0x0F00) >> 8);
  *outDuration = (uint8_t)(raw & 0x00FF);
}

static void calcMelodyDuration(Melody * melo) {
  uint16_t i;
  uint16_t ticksNeeded = 0;

  for (i = 0; i < melo->length; i++) {
    ticksNeeded += melo->seqPtr[i] & 0x00FF;
  }

  melo->duration = ticksNeeded + 1;
}

static void melodyFrame(Animation * anim) {
  uint8_t sNote;
  uint8_t sOctave;
  uint8_t sDuration;

  decodeStep(activeMelody->seqPtr[activeMelody->position],
            &sNote, &sOctave, &sDuration);

  if (activeMelody->stepPosition < sDuration) {
    if (activeMelody->stepPosition == 0) {
      playNote(sNote, sOctave);
    }

    activeMelody->stepPosition++;
    return;
  }

  activeMelody->stepPosition = 0;
  activeMelody->position++;

  if (activeMelody->position == activeMelody->length) {
    TONEGEN_OFF();
    activeMelody = NULL;
    return;
  }

  decodeStep(activeMelody->seqPtr[activeMelody->position],
            &sNote, &sOctave, &sDuration);

  playNote(sNote, sOctave);
  activeMelody->stepPosition++;
}

void playNote(tonegenNotes note, uint8_t octave) {
  if (note == Rest) {
    TONEGEN_OFF();
    return;
  }

  TONEGEN_ON();
  OCR1A = getCompValue((uint8_t)note, octave);
}

