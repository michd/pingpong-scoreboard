#include "stdint.h"
#ifndef PINGPONG_H_
 
#define PINGPONG_H_

void pingpongInit();
void pingpongGameLoop();
void pingpongPlayerButtonDown(uint8_t);
void pingpongPlayerButtonUp(uint8_t);
void pingpongModeButtonDown();
void pingpongModeButtonUp();

#endif /* PINGPONG_H_ */
