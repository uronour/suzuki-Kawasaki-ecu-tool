#include <stdio.h>
#include <math.h>
#include "gauge_dial.h"
#include "ili9488_gfx.h"
#include "lcd.h"

static int16_t g_sinTable[361];
static uint8_t g_tableReady = 0;

void DIAL_InitTable(void)
{
  if (g_tableReady) return;
  for (int i = 0; i <= 90; i++)
  {
    double rad = i * 3.141592653589793 / 180.0;
    g_sinTable[i] = (int16_t)(sin(rad) * DIAL_SIN_SCALE);
  }
  for (int i = 91; i <= 180; i++)
    g_sinTable[i] = g_sinTable[180 - i];
  for (int i = 181; i <= 360; i++)
    g_sinTable[i] = -g_sinTable[i - 180];
  g_tableReady = 1;
}

int16_t DIAL_Sin(int16_t deg)
{
  deg = deg % 360;
  if (deg < 0) deg += 360;
  return g_sinTable[deg];
}

int16_t DIAL_Cos(int16_t deg)
{
  return DIAL_Sin(deg + 90);
}

int16_t DIAL_AngleForValue(GaugeDial *d, uint32_t value)
{
  if (value <= d->minVal) return d->startAngle;
  if (value >= d->maxVal) return d->endAngle;
  uint32_t range = d->maxVal - d->minVal;
  uint32_t offset = value - d->minVal;
  int16_t sweep = (d->endAngle - d->startAngle + 360) % 360;
  int16_t ang = d->startAngle + (int16_t)((uint32_t)sweep * offset / range);
  return ang % 360;
}

void DIAL_DrawArcSweep(GaugeDial *d, int16_t fromAng, int16_t toAng, uint16_t color)
{
  int16_t sweep = (toAng - fromAng + 360) % 360;
  int16_t outer = d->radius;
  int16_t inner = d->radius - d->arcWidth;
  for (int16_t a = 0; a <= sweep; a++)
  {
    int16_t ang = (fromAng + a) % 360;
    int16_t cosv = DIAL_Cos(ang);
    int16_t sinv = DIAL_Sin(ang);
    for (int16_t r = inner; r <= outer; r++)
    {
      int16_t px = d->cx + (r * cosv) / DIAL_SIN_SCALE;
      int16_t py = d->cy + (r * sinv) / DIAL_SIN_SCALE;
      if (px >= 0 && px < LCD_WIDTH && py >= 0 && py < LCD_HEIGHT)
        LCD_DrawPixel(px, py, color);
    }
  }
}

void DIAL_DrawArcValue(GaugeDial *d, uint32_t fromVal, uint32_t toVal, uint16_t color)
{
  int16_t from = DIAL_AngleForValue(d, fromVal);
  int16_t to = DIAL_AngleForValue(d, toVal);
  DIAL_DrawArcSweep(d, from, to, color);
}

void DIAL_DrawTicks(GaugeDial *d, uint32_t tickEvery, uint32_t labelEvery, uint16_t tickColor, uint16_t labelColor)
{
  for (uint32_t v = d->minVal; v <= d->maxVal; v += tickEvery)
  {
    int16_t ang = DIAL_AngleForValue(d, v);
    int16_t cosv = DIAL_Cos(ang);
    int16_t sinv = DIAL_Sin(ang);
    int16_t outer = d->radius - d->arcWidth - 2;
    int16_t inner = (v % labelEvery == 0) ? (outer - 12) : (outer - 6);
    for (int16_t r = inner; r <= outer; r++)
    {
      int16_t px = d->cx + (r * cosv) / DIAL_SIN_SCALE;
      int16_t py = d->cy + (r * sinv) / DIAL_SIN_SCALE;
      if (px >= 0 && px < LCD_WIDTH && py >= 0 && py < LCD_HEIGHT)
        LCD_DrawPixel(px, py, tickColor);
    }
    if (v % labelEvery == 0)
    {
      int16_t lr = d->radius - d->arcWidth - 16;
      int16_t lx = d->cx + (lr * cosv) / DIAL_SIN_SCALE;
      int16_t ly = d->cy + (lr * sinv) / DIAL_SIN_SCALE;
      char buf[8];
      snprintf(buf, sizeof(buf), "%lu", (unsigned long)(v / 1000));
      int16_t tw = 8 * 4;
      GFX_DrawString(lx - tw / 2, ly - 6, buf, labelColor, COLOR_BLACK);
    }
  }
}

static void DrawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color)
{
  int16_t dx = (x1 > x0) ? (x1 - x0) : (x0 - x1);
  int16_t dy = (y1 > y0) ? (y1 - y0) : (y0 - y1);
  int16_t sx = (x1 >= x0) ? 1 : -1;
  int16_t sy = (y1 >= y0) ? 1 : -1;
  int16_t err = dx - dy;
  LCD_DrawPixel(x0, y0, color);
  while (x0 != x1 || y0 != y1)
  {
    int16_t e2 = 2 * err;
    if (e2 > -dy) { err -= dy; x0 += sx; }
    if (e2 < dx)  { err += dx; y0 += sy; }
    LCD_DrawPixel(x0, y0, color);
  }
}

static void DrawThickLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color, uint16_t innerColor, uint8_t width)
{
  if (width < 2) { DrawLine(x0, y0, x1, y1, color); return; }

  int16_t dx = x1 - x0;
  int16_t dy = y1 - y0;
  int16_t len = (dx < 0 ? -dx : dx) + (dy < 0 ? -dy : dy);
  if (len == 0) { LCD_DrawPixel(x0, y0, color); return; }

  int16_t perpX = -dy * width / 2 / len;
  int16_t perpY = dx * width / 2 / len;

  int16_t half = width / 2;
  for (int16_t w = -half; w <= half; w++)
  {
    int16_t wx = perpX * w / (half > 0 ? half : 1);
    int16_t wy = perpY * w / (half > 0 ? half : 1);

    uint16_t c;
    if (w == 0)
      c = color;
    else if (w == -half || w == half)
      c = innerColor;
    else
      c = (w == -1 || w == 1) ? color : innerColor;

    DrawLine(x0 + wx, y0 + wy, x1 + wx, y1 + wy, c);
  }
}

void DIAL_DrawNeedle(GaugeDial *d, uint32_t value, uint16_t color, uint16_t bgColor, uint32_t oldValue)
{
  int16_t tipR = d->radius - d->arcWidth - 4;
  int16_t tailR = -(d->radius / 5);

  if (value != oldValue && oldValue != 0xFFFFFFFF)
  {
    int16_t oldAng = DIAL_AngleForValue(d, oldValue);
    int16_t cosv = DIAL_Cos(oldAng);
    int16_t sinv = DIAL_Sin(oldAng);
    int16_t tx = d->cx + (tipR * cosv) / DIAL_SIN_SCALE;
    int16_t ty = d->cy + (tipR * sinv) / DIAL_SIN_SCALE;
    int16_t tlx = d->cx + (tailR * cosv) / DIAL_SIN_SCALE;
    int16_t tly = d->cy + (tailR * sinv) / DIAL_SIN_SCALE;

    DrawThickLine(tlx + 1, tly + 1, tx + 1, ty + 1, bgColor, bgColor, 7);
    DrawThickLine(tlx, tly, tx, ty, bgColor, bgColor, 7);
  }

  int16_t ang = DIAL_AngleForValue(d, value);
  int16_t cosv = DIAL_Cos(ang);
  int16_t sinv = DIAL_Sin(ang);
  int16_t tx = d->cx + (tipR * cosv) / DIAL_SIN_SCALE;
  int16_t ty = d->cy + (tipR * sinv) / DIAL_SIN_SCALE;
  int16_t tlx = d->cx + (tailR * cosv) / DIAL_SIN_SCALE;
  int16_t tly = d->cy + (tailR * sinv) / DIAL_SIN_SCALE;

  uint16_t shadowColor = RGB565(10, 10, 10);
  uint8_t r = (((color >> 11) & 0x1F) * 255 + 15) / 31;
  uint8_t g = (((color >> 5) & 0x3F) * 255 + 31) / 63;
  uint8_t b = ((color & 0x1F) * 255 + 15) / 31;
  uint16_t edgeColor = RGB565(r * 3 / 4, g * 3 / 4, b * 3 / 4);

  DrawThickLine(tlx + 1, tly + 1, tx + 1, ty + 1, shadowColor, shadowColor, 7);
  DrawThickLine(tlx, tly, tx, ty, color, edgeColor, 7);
}

void DIAL_DrawCenterHub(GaugeDial *d, uint16_t hubRadius, uint16_t color)
{
  int16_t r = hubRadius;
  int16_t cx = d->cx, cy = d->cy;
  for (int16_t y = -r; y <= r; y++)
    for (int16_t x = -r; x <= r; x++)
      if (x * x + y * y <= r * r)
        LCD_DrawPixel(cx + x, cy + y, color);
}
