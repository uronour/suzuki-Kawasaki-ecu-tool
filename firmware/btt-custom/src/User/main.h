#ifndef _MAIN_H_
#define _MAIN_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "stm32f2xx.h"

typedef struct {
  RCC_ClocksTypeDef rccClocks;
  uint32_t PCLK1_Timer_Frequency;
  uint32_t PCLK2_Timer_Frequency;
} CLOCKS;

extern CLOCKS mcuClocks;

int main(void);

#ifdef __cplusplus
}
#endif

#endif
