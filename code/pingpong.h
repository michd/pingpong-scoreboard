#include "stdint.h"
#include "button.h"

#ifndef PINGPONG_H_
 
#define PINGPONG_H_
// TODO: enums are a thing
#define PINGPONG_PLAYER_NONE 0
#define PINGPONG_PLAYER_1    1
#define PINGPONG_PLAYER_2    2

#define PINGPONG_DISPMODE_NONE 0
#define PINGPONG_DISPMODE_GAME 1
#define PINGPONG_DISPMODE_SET  2
#define PINGPONG_DISPMODE_ALL  3

void pingpongInit(Button *, Button *, Button *, uint16_t, uint16_t);
void pingpongGameTick();
void pingpongButtonPress(Button *);
void pingpongButtonLongPress(Button *);
void pingpongSetMode(uint8_t);
void pingpongIndicatePlayerTurn(uint8_t);

#endif /* PINGPONG_H_ */
