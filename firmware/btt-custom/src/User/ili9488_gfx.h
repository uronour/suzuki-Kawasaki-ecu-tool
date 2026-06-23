#ifndef _ILI9488_GFX_H_
#define _ILI9488_GFX_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

#ifndef LCD_WIDTH
  #define LCD_WIDTH  480
#endif
#ifndef LCD_HEIGHT
  #define LCD_HEIGHT 320
#endif

#define RGB565(r,g,b) ((((r>>3)<<11) | ((g>>2)<<5) | (b>>3)))

#define COLOR_BLACK   0x0000
#define COLOR_WHITE   0xFFFF
#define COLOR_RED     0xF800
#define COLOR_GREEN   0x07E0
#define COLOR_BLUE    0x001F
#define COLOR_YELLOW  0xFFE0
#define COLOR_CYAN    0x07FF
#define COLOR_MAGENTA 0xF81F
#define COLOR_GRAY    0x8410
#define COLOR_DARK    0x18E3
#define COLOR_ORANGE  0xFD20

void LCD_Fill(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t color);
void LCD_DrawPixel(uint16_t x, uint16_t y, uint16_t color);
void LCD_DrawHLine(uint16_t x0, uint16_t x1, uint16_t y, uint16_t color);
void LCD_DrawVLine(uint16_t x, uint16_t y0, uint16_t y1, uint16_t color);
void LCD_DrawRect(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t color);
void LCD_FillRect(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t color);
void LCD_DrawProgressBar(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint8_t pct, uint16_t barColor, uint16_t bgColor);
void LCD_Backlight_On(void);
void LCD_SetBrightness(uint8_t pct);

void GFX_DrawChar(int16_t x, int16_t y, char c, uint16_t color, uint16_t bg);
void GFX_DrawString(int16_t x, int16_t y, const char *str, uint16_t color, uint16_t bg);
void GFX_DrawStringCenter(int16_t y, const char *str, uint16_t color, uint16_t bg);
void GFX_DrawInt(int16_t x, int16_t y, int32_t val, uint8_t digits, uint16_t color, uint16_t bg);
void GFX_DrawFloat(int16_t x, int16_t y, float val, uint8_t decimals, uint16_t color, uint16_t bg);

void GFX_DrawCharScaled(int16_t x, int16_t y, char c, uint16_t color, uint16_t bg, uint8_t scale);
void GFX_DrawStringScaled(int16_t x, int16_t y, const char *str, uint16_t color, uint16_t bg, uint8_t scale);

#ifdef __cplusplus
}
#endif

#endif
