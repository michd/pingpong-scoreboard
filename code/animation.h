#ifndef ANIMATION_H_
#define ANIMATION_H_

#include "stdint.h"

struct Animation;
typedef void (*animatorFunction)(struct Animation *);

typedef struct {
  uint16_t stepTicks; // Number of ticks between "frames"
  int32_t duration; // Number of frames, -1 for infinite
  uint32_t position; // How many ticks into the animation
  animatorFunction frame;
} Animation;

void animationTick(uint32_t);
void animationSetActive(Animation *);
void animationClear();

#endif
