#ifndef BUTTON_H_
#define BUTTON_H_

#include "stdint.h"
#include "stdbool.h"

// Describes a button, abstracting the implementation details for debounce,
// press, and long press so pingpong.c can focus mostly on game logic
typedef struct {
  // Port A pin this button is for
  uint8_t pin;

  // Tick count when this was last has a debounced down-going flank
  uint64_t lastDown;

  // Tick count when this last triggered a "pressed" call
  uint64_t lastPress;

  // Whether the button is currently down
  bool down;

  // Whether the button is currently down and has been held a "long" time
  bool held;
} Button;

#endif // BUTTON_H_
