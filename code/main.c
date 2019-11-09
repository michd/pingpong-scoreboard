#define F_CPU 16000000UL

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include "MAX72S19.h"
#include "pingpong.h"

#define PIN_BTN_PLAYER1 PINA1
#define PIN_BTN_PLAYER2 PINA2
#define PIN_BTN_MODE    PINA3
#define PIN_DISP_DATA   PINA4
#define PIN_DISP_CLK    PINA5
#define PIN_DISP_CS     PINA6

#define PINA_CHANGED(pin) ((PINA & (1 << (pin))) != (portACache & (1 << (pin))))
#define READ_PINA(pin) (PINA & (1 << (pin)))

void ioSetup();
void onPinChangeA(uint8_t pin);
void writeNumber(uint16_t);

uint8_t portACache;

int main (void) {
  uint16_t n = 0;
  ioSetup();
  PORTA |= 0x01;

  pingpongInit();
  pingpongGameLoop();
}

void ioSetup() {
  // Port A configuration
  // 1 = output in DDRx
  // A:7654 3210
  //   0111 0001: 0x71 - Pin A7 is unused.
  //                     Pins A6-A4 are outputs for communicating w/display.
  //                     Pins A3-A1 are inputs for buttons (w/ interrupt).
  //                     Pin  A0 is an output for an LED, for debugging.
  DDRA = 0x71;

  // If a pin is an input and the PORTx bit is 1, pullup is enabled
  // If it is an output the PORTx sets that output level, 1 or 0
  // A:7654 3210
  //   0111 1110 :0x0E - Pins A3-A1 are inputs with pullups, for buttons
  PORTA |= 0x0E | (1 << PIN_DISP_CS);

  // A1: PCINT1
  // A2: PCINT2
  // A3: PCINT3
  // All these are on Pin change interrupt 0

  // Enable interrupt request for interrupt 0
  // Datasheet 9.3.2
  GIMSK |= (1 << (PCIE0));

  // Enable pin changes for pins we're interested in, for interrupt 0
  // Datasheet 9.3.5
  PCMSK0 |= (1 << PCINT1) | (1 << PCINT2) | (1 << PCINT3);

  // Globally enable interrupts. pretty important.
  sei();

  portACache = PINA;
  displaySetup(PIN_DISP_CS, PIN_DISP_DATA, PIN_DISP_CLK);
  displaySetDecodeMode(0x00);
  displaySetIntensity(0xF);
  displaySetScanLimit(5); // 5th digit used for some extra indication LEDs
  displayClear();
  displayActivate();
}

// Interrupt vector 0 triggered
// This vector is used for pin change interrupts on port A
ISR(PCINT0_vect) {
  for (uint8_t i = 0; i < 8; i++) {
    if (PINA_CHANGED(i)) {
      onPinChangeA(i);
      portACache = PINA;
      break;
    }
  }

  portACache = PINA;
}

void onPinChangeA(uint8_t pin) {
  uint8_t player;

  if (pin == PIN_BTN_PLAYER1 || pin == PIN_BTN_PLAYER2) {
    player = pin == PIN_BTN_PLAYER1 ? 1 : 2;

    if (READ_PINA(pin)) {
      pingpongPlayerButtonUp(player);
    } else {
      pingpongPlayerButtonDown(player);
    }

  } else if (pin == PIN_BTN_MODE) {
    if (READ_PINA(pin)) {
      pingpongModeButtonUp();
    } else {
      pingpongModeButtonDown();
    }
  }
}

// FIXME: doesn't clear digits that it's not writing
void writeNumber(uint16_t n) {
  uint16_t rem = n;
  uint8_t digitIndex = 0;

  do {
    displayWriteNumber(digitIndex, rem % 10);

    rem /= 10;
    digitIndex++;
  } while (rem);

}
