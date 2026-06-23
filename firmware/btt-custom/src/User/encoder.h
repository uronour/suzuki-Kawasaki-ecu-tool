#ifndef _ENCODER_H_
#define _ENCODER_H_

#include <stdint.h>

void Encoder_Init(void);
void Encoder_Poll(void);
int8_t Encoder_GetDir(void);
uint8_t Encoder_GetPress(void);

#endif
