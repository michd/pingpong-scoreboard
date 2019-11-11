#include "animation.h"
#include "stdint.h"
#include "stddef.h"

static Animation * activeAnimation;

void animationTick(uint32_t ticks) {
  Animation * anim = activeAnimation;

  if (anim == NULL) return;

  if (ticks % anim->stepTicks != 0) return;

  anim->frame(anim);

  if (anim->duration == anim->position) {
    if (anim->finished != NULL) anim->finished(anim);
    activeAnimation = NULL;
  }
}

void animationSetActive(Animation * anim) {
  activeAnimation = anim;
}

void animationClear() {
  activeAnimation = NULL;
}

