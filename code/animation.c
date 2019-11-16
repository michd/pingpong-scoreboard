#include "animation.h"
#include "stdint.h"
#include "stddef.h"
#include "stdbool.h"
#include "MAX72S19.h"
#include "pingpong.h"

static Animation * _activeAnimation;

static Animation _startupAnim, _player1WonAnim, _player2WonAnim,
                 _buttonPressAnim, _buttonLongPressAnim;

static void _startupFrame(Animation *);
static void _player1WinFrame(Animation *);
static void _player2WinFrame(Animation *);
static void _winFrame(Animation *, uint8_t);
static void _buttonPressFrame(Animation *);
static void _buttonLongPressFrame(Animation *);

void animationInit() {
  // Set up animation structs
  _startupAnim.stepTicks = 25;
  _startupAnim.duration = 0x4F;
  _startupAnim.frame = (animatorFunction)_startupFrame;

  _player1WonAnim.stepTicks = 100;
  _player1WonAnim.duration = 20;
  _player1WonAnim.frame = (animatorFunction)_player1WinFrame;

  _player2WonAnim.stepTicks = 100;
  _player2WonAnim.duration = 20;
  _player2WonAnim.frame = (animatorFunction)_player2WinFrame;

  // TODO: other animations, further setup
  _buttonPressAnim.frame = (animatorFunction)_buttonPressFrame;

  _buttonLongPressAnim.frame = (animatorFunction)_buttonLongPressFrame;
}

void animationTick(uint32_t ticks) {
  Animation * anim = _activeAnimation;

  if (anim == NULL) return;

  if (ticks % anim->stepTicks != 0) return;

  if (anim->duration == anim->position) {
    _activeAnimation = NULL;
    return;
  }

  anim->frame(anim);
  anim->position++;
}

void animationSetActive(Animation * anim) {
  _activeAnimation = anim;
}

void animationClear() {
  if (_activeAnimation == NULL) return;

  // Skip to final frame, essentially. Letting the animation wrap up.
  _activeAnimation->position = _activeAnimation->duration - 1;
  _activeAnimation->frame(_activeAnimation);

  _activeAnimation = NULL;
}

void animationTrigger(Animations animEnum) {
  Animation * anim = NULL;

  switch (animEnum) {
    case Startup: anim = &_startupAnim; break;
    case Player1Win: anim = &_player1WonAnim; break;
    case Player2Win: anim = &_player2WonAnim; break;
    case ButtonPress: anim = &_buttonPressAnim; break;
    case ButtonLongPress: anim = &_buttonLongPressAnim; break;
  }

  if (anim != NULL) {
    anim->position = 0;
    animationSetActive(anim);
  }
}

// Animation implementations ---------------------------------------------------


static void _startupFrame(Animation * a) {
  uint32_t pos = a->position;

  if (pos == 0) {
    displayWriteChar(3, 'P', false);
    displayWriteChar(2, 'i', false);
    displayWriteChar(1, 'n', false);
    displayWriteChar(0, 'g', false);
  } else  if (pos == 0x20) {
    displayWriteChar(2, 'o', false);
  } else if (pos >= 0x40) {
    pingpongSetMode(PINGPONG_DISPMODE_GAME);
  }

  // Note actual position is incremented by animation system
  pos++;

  if (pos < 0x10
      || (pos >= 0x20 && pos < 0x30)
      || (pos >= 0x40)) {
    displaySetIntensity(pos % 0x10);
  } else {
    displaySetIntensity(0xF - (pos % 0x10));
  }
}

static void _player1WinFrame(Animation * a) {
  _winFrame(a, PINGPONG_PLAYER_1);
}

static void _player2WinFrame(Animation * a) {
  _winFrame(a, PINGPONG_PLAYER_2);
}

static void _winFrame(Animation * a, uint8_t player) {
  bool ledOn = a->position % 2 == 0;
  pingpongIndicatePlayerTurn(ledOn ? player : PINGPONG_PLAYER_NONE);
}

static void _buttonPressFrame(Animation * a) {

}

static void _buttonLongPressFrame(Animation * a) {

}
