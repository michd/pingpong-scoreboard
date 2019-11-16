#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/eeprom.h>
#include <util/delay.h>
#include "MAX72S19.h"
#include "button.h"
#include "pingpong.h"
#include "animation.h"
#include "tonegen.h"
#include "stdbool.h"
#include "stdint.h"

#define PIN_BTN_PLAYER1 PINA1
#define PIN_BTN_PLAYER2 PINA2
#define PIN_BTN_MODE    PINA3
#define PIN_DISP_DATA   PINA4
#define PIN_DISP_CLK    PINA5
#define PIN_DISP_CS     PINA7
// Pin A6 used for timer 1 output compare match A output

#define TICK_MS 2
#define BTN_DEBOUNCE_TICKS 25
#define BTN_PRESS_TICKS 1
#define BTN_LONG_PRESS_TICKS 750

#define PINA_CHANGED(p) ((PINA & (1 << (p))) != (_portACache & (1 << (p))))
#define READ_PINA(p) (PINA & (1 << (p)))
#define BTN_FOR_PIN(p) ((p==PINA1) \
                          ? &_buttons[0] \
                          : (p==PINA2) \
                            ? &_buttons[1] \
                            : &_buttons[2])

#define EEPROM_ADDR_SCORE_P1 (0x00)
#define EEPROM_ADDR_SCORE_P2 (0x02)

#define DEBUG_LED_ON (PORTA |= 0x01);
#define DEBUG_LED_OFF (PORTA &= ~(0x01));
#define DEBUG_TOGGLE_LED (PORTA = (PORTA & 0xFE) | ~(PORTA & 0x01))

static void _ioSetup();
static void _timerSetup();
static void _onPinChangeA(uint8_t pin);
static void _checkButtons();
static void _tick();

static Button _buttons[3];
static uint32_t _ticks;
static uint8_t _portACache;

int main (void) {
  _ioSetup();
  _timerSetup();
  animationInit();

  // References to buttons for player 1, 2, and mode button
  pingpongInit(
      &_buttons[0], &_buttons[1], &_buttons[2],
      EEPROM_ADDR_SCORE_P1,
      EEPROM_ADDR_SCORE_P2);

  // Globally enable interrupts. pretty important.
  sei();

  // Everything done via interrupts from this point
  while (1);
}

static void _ioSetup() {
  // Port A configuration
  // 1 = output in DDRx
  // A:7654 3210
  //   1111 0001: 0xF1 - Pin A7(out) is used for display serial communication
  //                     Pin A6(out) is used for timer1 output compare match A
  //                     Pins A5, A4(out) are used for display serial comm.
  //                     Pins A3-A1(in) are for buttons (w/ interrupt).
  //                     Pin A0(out) is a debug LED.
  DDRA = 0xF1;

  // If a pin is an input and the PORTx bit is 1, pullup is enabled
  // If it is an output the PORTx sets that output level, 1 or 0
  // A:7654 3210
  //   0000 1110 :0x0E - Pins A3-A1 are inputs with pullups, for buttons
  PORTA |= 0x0E | (1 << PIN_DISP_CS); // Also setting chip select high

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

  _portACache = PINA;

  _buttons[0].pin = PIN_BTN_PLAYER1;
  _buttons[1].pin = PIN_BTN_PLAYER2;
  _buttons[2].pin = PIN_BTN_MODE;

  displaySetup(PIN_DISP_CS, PIN_DISP_DATA, PIN_DISP_CLK);
  displaySetDecodeMode(0x00);
  displaySetIntensity(0xF);
  displaySetScanLimit(6); // 5th and 6th digit used for some indication LEDs
  displayClear();
  displayActivate();
}

static void _timerSetup() {
  //----------------------------------------------------------------------------
  // Timer / Counter 0 - used for a timing tick every 2ms, produces interrupt
  //----------------------------------------------------------------------------

  // Timer / Counter 0 Control Register A
  TCCR0A = 0x02;
  // 0000 0010 : 0x02
  // |||| ||\\- WGM00, WGM01, Set for CTC (clear timer on compare)
  // |||| \\- Unused
  // ||\\- COM0B1, COM0B0: Compare match output B, disconnectoed
  // \\- COM0A1, COM0A0: Compare match output A, disconnected

  // Timer / Counter 0 Control Register B
  TCCR0B = 0x04;
  // 0000 0100 : 0x04
  // |||| |\\\- CS02, CS01, CS00: Clock select: Main clock / 256
  // |||| \- WGM02
  // ||\\- Reserved, unused
  // \\- FOC0A, FOC0B: Force output compare A/B: irrelevant

  // In TCCR0B, Timer 0's prescaler is set to use a prescaler of 256.
  // This means that the counter counts at a frequency of F_CPU / 256.
  // We're using 16 MHz, so the counter frequency will be 62.5 kHz

  // Timer / Counter 0 Output Compare Register A
  OCR0A = 124;

  // The output compare register contains the value of the counter at which
  // we'll do something. In this case, we'll generate an interrupt.
  // At 62.5 kHz, this means the interrupt will be generated 500 times per
  // second, or once every 2 milliseconds.

  // Timer / Counter 0 Interrupt Mask register
  TIMSK0 = 0x02;
  // 0000 0010 : 0x02
  // |||| |||\- TOIE0: Timer/Counter 0 overflow interrupt enable: off
  // |||| ||\- OC1E0A: Timer/Counter 0 output compare match A enable: on
  // |||| |\- OC1E0B: Timer/Counter 0 output compare match B enable: off
  // \\\\ \- Reserved, unused.

  // In TIMSK0, we're enabling the interrupt for Output compare A. in OCR0A
  // we set the value to compare to, here we're making sure an interrupt will
  // be triggered when Timer/Counter 0's counter value matches what's in there.

  //----------------------------------------------------------------------------
  // Timer / Counter 1 - used for sound output, producing square waves
  //----------------------------------------------------------------------------

  // Timer / Counter 1 Control Register A
  TCCR1A = 0x00;
  // 0000 0000 : 0x00
  // |||| ||\\- WGM11:10: Part of WGM13:10, set to mode 4: CTC with TOP=OCR1A
  // |||| ||              Timer counter will be cleared when it matches output
  // |||| ||              compare register A. This being on makes it so that
  // |||| ||              output compare register combined with the prescaler
  // |||| ||              determine the resulting square wave frequency.
  // |||| \\- Unused
  // ||\\- COM1B1:0: Normal operation, OC1B disconnected: no pin toggling for
  // ||              output compare register B
  // \\- COM1A1:0: Toggle OC1A on Compare Match: Disconnected
  //               Turn this on with notegenToggle(true), will set this to 01.
  //               The OC1A (PA6) pin will then be toggled every time the output
  //               compare register A matches the counter value.

  // Timer / Counter 1 Control Register B
  TCCR1B = 0x0A;
  // 0000 1010 : 0x0A
  // |||| |\\\- CS12:10: Clock select: Main clock / 8
  // |||\ \- WGM13:12: Part of WGM13:10, set to mode 4. See comment for TCCR1A
  // |||               WGM11:10 bits.
  // ||\- Unused
  // |\- ICES1: Input Capture Edge Select: off, irrelevant
  // \- ICNC1: Input Capture Noise Canceler: off, irrelevant

  // Timer / Counter 1 Control Register C
  TCCR1C = 0x00;
  // 0000 0000 : 0x00
  // ||\\ \\\\- Unused
  // \\- FOC1A, FOC1B: Force Output Compate for Channel A, B: irrelevant, only
  //                   relevant in PWM modes, which we aren't using.

  // Output Compare Register 1 A
  OCR1A = 2273; // Set for a frequency of ~440.Hz at the output pin, for testing
  // Later on this should be set up to be quiet initially, and be dynamically
  // changed for playing tunes.
}

static void _onPinChangeA(uint8_t pin) {
  Button * btn = BTN_FOR_PIN(pin);

  if (READ_PINA(pin)) {
    // Don't care about positive flanks here, button down is cleared in
    // _checkButtons are required
    return;
  }

  btn->down = true;

  if (_ticks - btn->lastDown > BTN_DEBOUNCE_TICKS) {
    btn->lastDown = _ticks;
  }
}

static void _checkButtons() {
  uint8_t i;
  bool wasDown;
  bool wasHeld;

  Button * btn;

  for (i = 0; i < sizeof(_buttons) / sizeof(Button); i++) {
    btn = &_buttons[i];
    wasDown = btn->down;
    wasHeld = btn->held;

    if (READ_PINA(btn->pin)) {
      btn->down = false;
      btn->held = false;

      // Register the (short) press if the button was down for more than
      // press ticks, but less than hold ticks, and is now released
      if (
          wasDown && !wasHeld && 
          _ticks - btn->lastPress >= BTN_DEBOUNCE_TICKS &&
          _ticks - btn->lastDown >= BTN_PRESS_TICKS) {
        pingpongButtonPress(btn);
        btn->lastPress = _ticks;
      }

      continue;
    }

    if (_ticks - btn->lastDown > BTN_LONG_PRESS_TICKS && !btn->held) {
      btn->held = true;
      pingpongButtonLongPress(btn);
    }
  }
}

static void _tick() {
  _ticks++;
  _checkButtons();
  animationTick(_ticks);
  pingpongGameTick();
}

// Interrupt vector 0 triggered
// This vector is used for pin change interrupts on port A
ISR(PCINT0_vect) {
  for (uint8_t i = 0; i < 8; i++) {
    if (PINA_CHANGED(i)) {
      _onPinChangeA(i);
      _portACache = PINA;
      break;
    }
  }

  _portACache = PINA;
}

// Interrupt vector for Timer 0 output compare match A triggered
// Used for a 2ms tick for timing things that could do with timing
ISR(TIM0_COMPA_vect) {
  _tick();
}
