#include <stdio.h>
#include "ili9488_gfx.h"
#include "lcd.h"
#include "LCD_Driver/ILI9488.h"
#include "timer_pwm.h"
#include "GPIO_Init.h"
#include "variants.h"
#include "font_8x13.h"

void LCD_Fill(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t color)
{
  uint32_t pixels = (uint32_t)(x1 - x0 + 1) * (y1 - y0 + 1);
  ILI9488_SetWindow(x0, y0, x1, y1);
  LCD_WR_REG(0x2C);
  for (uint32_t i = 0; i < pixels; i++)
    LCD_WR_DATA(color);
}

void LCD_DrawPixel(uint16_t x, uint16_t y, uint16_t color)
{
  ILI9488_SetWindow(x, y, x, y);
  LCD_WR_REG(0x2C);
  LCD_WR_DATA(color);
}

void LCD_DrawHLine(uint16_t x0, uint16_t x1, uint16_t y, uint16_t color)
{
  if (x0 > x1) { uint16_t t = x0; x0 = x1; x1 = t; }
  uint16_t count = x1 - x0 + 1;
  ILI9488_SetWindow(x0, y, x1, y);
  LCD_WR_REG(0x2C);
  while (count--) LCD_WR_DATA(color);
}

void LCD_DrawVLine(uint16_t x, uint16_t y0, uint16_t y1, uint16_t color)
{
  if (y0 > y1) { uint16_t t = y0; y0 = y1; y1 = t; }
  uint16_t count = y1 - y0 + 1;
  ILI9488_SetWindow(x, y0, x, y1);
  LCD_WR_REG(0x2C);
  while (count--) LCD_WR_DATA(color);
}

void LCD_DrawRect(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t color)
{
  LCD_DrawHLine(x0, x1, y0, color);
  LCD_DrawHLine(x0, x1, y1, color);
  LCD_DrawVLine(x0, y0, y1, color);
  LCD_DrawVLine(x1, y0, y1, color);
}

void LCD_FillRect(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t color)
{
  LCD_Fill(x0, y0, x1, y1, color);
}

void LCD_DrawProgressBar(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint8_t pct, uint16_t barColor, uint16_t bgColor)
{
  if (pct > 100) pct = 100;
  LCD_FillRect(x, y, x + w, y + h, bgColor);
  uint16_t fillW = ((w - 2) * pct) / 100;
  if (fillW > 0)
    LCD_FillRect(x + 1, y + 1, x + 1 + fillW, y + h - 1, barColor);
}

void LCD_Backlight_On(void)
{
  GPIO_InitSet(LCD_LED_PIN, MGPIO_MODE_AF_PP, LCD_LED_PIN_ALTERNATE);
  TIM_PWM_Init(LCD_LED_PWM_CHANNEL);
  TIM_PWM_SetDutyCycle(LCD_LED_PWM_CHANNEL, 100);
}

void LCD_SetBrightness(uint8_t pct)
{
  TIM_PWM_SetDutyCycle(LCD_LED_PWM_CHANNEL, pct);
}

static const uint8_t (*g_font)[13] = font_8x13;

void GFX_DrawChar(int16_t x, int16_t y, char c, uint16_t color, uint16_t bg)
{
  if (c < 32 || c > 126) c = 32;
  uint8_t idx = c - 32;
  for (uint8_t row = 0; row < FONT_H; row++)
  {
    uint8_t bits = g_font[idx][row];
    for (uint8_t col = 0; col < FONT_W; col++)
    {
      if (bits & (0x80 >> col))
        LCD_DrawPixel(x + col, y + row, color);
    }
  }
}

void GFX_DrawString(int16_t x, int16_t y, const char *str, uint16_t color, uint16_t bg)
{
  while (*str)
  {
    GFX_DrawChar(x, y, *str++, color, bg);
    x += FONT_STEP;
  }
}

void GFX_DrawStringCenter(int16_t y, const char *str, uint16_t color, uint16_t bg)
{
  int16_t len = 0;
  const char *p = str;
  while (*p++) len++;
  int16_t x = (LCD_WIDTH - len * FONT_STEP) / 2;
  if (x < 0) x = 0;
  GFX_DrawString(x, y, str, color, bg);
}

void GFX_DrawStringCenterScaled(int16_t y, const char *str, uint16_t color, uint16_t bg, uint8_t scale)
{
  int16_t len = 0;
  const char *p = str;
  while (*p++) len++;
  int16_t x = (LCD_WIDTH - len * FONT_STEP * scale) / 2;
  if (x < 0) x = 0;
  GFX_DrawStringScaled(x, y, str, color, bg, scale);
}

void GFX_DrawInt(int16_t x, int16_t y, int32_t val, uint8_t digits, uint16_t color, uint16_t bg)
{
  char buf[16];
  snprintf(buf, sizeof(buf), "%*ld", digits, (long)val);
  GFX_DrawString(x, y, buf, color, bg);
}

void GFX_DrawFloat(int16_t x, int16_t y, float val, uint8_t decimals, uint16_t color, uint16_t bg)
{
  char buf[16];
  snprintf(buf, sizeof(buf), "%.*f", decimals, val);
  GFX_DrawString(x, y, buf, color, bg);
}

void GFX_DrawCharScaled(int16_t x, int16_t y, char c, uint16_t color, uint16_t bg, uint8_t scale)
{
  if (c < 32 || c > 126) c = 32;
  uint8_t idx = c - 32;
  for (uint8_t row = 0; row < FONT_H; row++)
  {
    uint8_t bits = g_font[idx][row];
    for (uint8_t col = 0; col < FONT_W; col++)
    {
      if (bits & (0x80 >> col))
      {
        for (uint8_t dy = 0; dy < scale; dy++)
          for (uint8_t dx = 0; dx < scale; dx++)
            LCD_DrawPixel(x + col * scale + dx, y + row * scale + dy, color);
      }
    }
  }
}

void GFX_DrawStringScaled(int16_t x, int16_t y, const char *str, uint16_t color, uint16_t bg, uint8_t scale)
{
  while (*str)
  {
    GFX_DrawCharScaled(x, y, *str++, color, bg, scale);
    x += FONT_STEP * scale;
  }
}
