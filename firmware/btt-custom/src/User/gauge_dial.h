#ifndef _GAUGE_DIAL_H_
#define _GAUGE_DIAL_H_

#include <stdint.h>

#define DIAL_SIN_SCALE 1024

typedef struct {
  int16_t cx, cy;
  uint16_t radius;
  int16_t startAngle;
  int16_t endAngle;
  uint32_t minVal;
  uint32_t maxVal;
  uint16_t arcWidth;
} GaugeDial;

void DIAL_InitTable(void);
int16_t DIAL_Cos(int16_t deg);
int16_t DIAL_Sin(int16_t deg);
int16_t DIAL_AngleForValue(GaugeDial *d, uint32_t value);
void DIAL_DrawArcSweep(GaugeDial *d, int16_t fromAng, int16_t toAng, uint16_t color);
void DIAL_DrawArcValue(GaugeDial *d, uint32_t fromVal, uint32_t toVal, uint16_t color);
void DIAL_DrawTicks(GaugeDial *d, uint32_t tickEvery, uint32_t labelEvery, uint16_t tickColor, uint16_t labelColor);
void DIAL_DrawNeedle(GaugeDial *d, uint32_t value, uint16_t color, uint16_t bgColor, uint32_t oldValue);
void DIAL_DrawCenterHub(GaugeDial *d, uint16_t hubRadius, uint16_t color);

#endif
