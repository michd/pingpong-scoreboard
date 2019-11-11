/*
 * MAX72S19c
 *
 * Created: /11/2017 12:44:51
 *  Author: Mich
 */ 

#include "MAX72S19.h"
#define _BV(bit) (1 << (bit))
#define constrain(amt,low,high) ((amt)<(low)?(low):((amt)>(high)?(high):(amt)))

static uint8_t _pinChipSelect;
static uint8_t _pinDataOut;
static uint8_t _pinClock;

static uint8_t _mapChar(char inputChar);
static void _beginTransmission();
static void _endTransmission();
static void _setDigitRegister(uint8_t reg, uint8_t data);
static void _setRegister(uint8_t reg, uint8_t data);
static void _shiftOut(uint8_t data);

static uint8_t _digitCache[MAX_DIGITS];

void displaySetup(uint8_t pinChipSelect, uint8_t pinDataOut, uint8_t pinClock) {
	_pinChipSelect = pinChipSelect;
	_pinDataOut = pinDataOut;
	_pinClock = pinClock;	
	
	DDRA |= _BV(_pinChipSelect) | _BV(_pinDataOut) | _BV(_pinClock);
	PORTA &= ~(_BV(_pinDataOut) | _BV(_pinClock));
	PORTA |= _BV(_pinChipSelect);

  for (uint8_t i = 0; i < MAX_DIGITS; i++) _digitCache[i] = 0x00;
}


void displaySetLED(uint8_t row, uint8_t column, bool on) {
  uint8_t newRowData = _digitCache[row];

  if (on) {
    newRowData |= (1 << column);
  } else {
    newRowData &= ~(1 << column);
  }

  _setDigitRegister(REG_DIGIT0 + row, newRowData);
}

void displaySetRow(uint8_t row, uint8_t states) {
  _setDigitRegister(REG_DIGIT0 + row, states);
}

void displaySetColumn(uint8_t column, uint8_t states) {
  for (uint8_t y = 0; y < 8; y++) {
    _setDigitRegister(
        REG_DIGIT0 + y,
        _digitCache[y - REG_DIGIT0] & ((~(1 << y)) | ((1 << y) & states)));
  }
}

void displayClear() {
  for (uint8_t i = 0; i < MAX_DIGITS; i++) {
    // Not using _setDigitRegister, because for clearing we don't want
    // to rely on the cache being correct. Instead, we make sure to clear
    // the display cache here, though.
    _setRegister(REG_DIGIT0 + i, 0x00);
    _digitCache[i] = 0x00;
  }
}

void displayWrite(uint8_t reg, uint8_t value) {
	_setDigitRegister(reg, value);
}

void displayWriteChar(uint8_t digitIndex, char character, bool dotOn) {
	digitIndex = constrain(digitIndex, 0, 7);
	uint8_t value = _mapChar(character);

	if (dotOn) {
		value |= 0b10000000;
	}

	_setDigitRegister(REG_DIGIT0 + digitIndex, value);
}

void displayWriteNumber(uint8_t digitIndex, uint8_t number) {
	number = constrain(number, 0, 9);
	displayWriteChar(digitIndex, '0' + number, false);
}

void displayPrint(uint8_t startDigitIndex, char characters[]) {
	uint8_t curDigit = constrain(startDigitIndex, 0, 7);
	uint8_t charIndex = 0;
	char curChar = characters[charIndex];

	while (curDigit < MAX_DIGITS * 2) {
		if (curChar == '\0') break;
		bool curCharDot = characters[charIndex + 1] == '.';
		displayWriteChar(curDigit++, curChar, curCharDot);
		if (curCharDot) ++charIndex;
		curChar = characters[++charIndex];
	}
}

void displaySetDecodeMode(uint8_t modes) {
	_setRegister(REG_DECODEMODE, modes);
}

void displaySetIntensity(uint8_t intensity) {
	_setRegister(REG_INTENSITY, constrain(intensity, 0x0, 0xF));
}

void displaySetScanLimit(uint8_t scanLimit) {
	_setRegister(REG_SCANLIMIT, constrain(scanLimit, 0x0, 0xF));
}

void displayStartDisplayTest() {
	_setRegister(REG_DISPLAYTEST, 1);
}

void displayStopDisplayTest() {
	_setRegister(REG_DISPLAYTEST, 0);
}

void displayShutdown() {
	_setRegister(REG_SHUTDOWN, 0);
}

void displayActivate() {
	_setRegister(REG_SHUTDOWN, 1);
}

// Private methods

static uint8_t _mapChar(char inputChar) {
  switch (inputChar) {
    //  Segments legend:
    //          _______
    //        /   A   /
    //     F /       / B
    //      /_______/
    //     /   G   /
    //  E /       / C
    //   /_______/ * dp
    //       D
    //                dpABCDEFG
    case '0': return 0b01111110;
    case '1': return 0b00110000;
    case '2': return 0b01101101;
    case '3': return 0b01111001;
    case '4': return 0b00110011;
    case '5': return 0b01011011;
    case '6': return 0b01011111;
    case '7': return 0b01110000;
    case '8': return 0b01111111;
    case '9': return 0b01111011;
    case 'P': return 0b01100111;
    case 'i': return 0b00010000;
    case 'n': return 0b00010101;
    case 'g': return 0b01111011;
    case 'o': return 0b00011101;
    default:  return 0b00000000;
  }
}

static void _beginTransmission() {
	PORTA &= ~(_BV(_pinChipSelect) | _BV(_pinClock));
}

static void _endTransmission() {
	PORTA |= _BV(_pinChipSelect);
	PORTA &= ~_BV(_pinClock);
}

static void _setDigitRegister(uint8_t reg, uint8_t data) {
  if (reg < REG_DIGIT0 || reg > REG_DIGIT7) return;

  // Don't write if value matches cached value
  if (data == _digitCache[reg - REG_DIGIT0]) return;
  _digitCache[reg - REG_DIGIT0] = data;

  _setRegister(reg, data);
}

static void _setRegister(uint8_t reg, uint8_t data) {
	_beginTransmission();
	_shiftOut(reg);
	_shiftOut(data);
	_endTransmission();
}

static void _shiftOut(uint8_t data) {
	for (uint8_t i = 0; i < 8; i++) {
		uint8_t val = !!(data & _BV(7 - i));
		
		if (val) {
			PORTA |= _BV(_pinDataOut);
		} else {
			PORTA &= ~_BV(_pinDataOut);
		}
		
		PORTA |= _BV(_pinClock);
		// TODO If this appears to be too fast, add a delay here
		PORTA &= ~_BV(_pinClock);
	}	
}
