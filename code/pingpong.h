#include "stdint.h"
#include "button.h"

#ifndef PINGPONG_H_
 
#define PINGPONG_H_

#define PINGPONG_PLAYER_NONE 0
#define PINGPONG_PLAYER_1    1
#define PINGPONG_PLAYER_2    2

void pingpongInit(Button *, Button *, Button *);
void pingpongGameTick();
void pingpongButtonPress(Button *);
void pingpongButtonLongPress(Button *);

#endif /* PINGPONG_H_ */
