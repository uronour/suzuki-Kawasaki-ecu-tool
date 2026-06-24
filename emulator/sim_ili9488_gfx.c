#include "ili9488_gfx.h"
#undef RGB565
#undef COLOR_ORANGE
#include "sdl_gfx.h"
#include "font_8x13.h"

void LCD_Fill(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t color)
{
    SDL_GFX_FillRect(x0, y0, x1, y1, color);
}

void LCD_DrawPixel(uint16_t x, uint16_t y, uint16_t color)
{
    SDL_GFX_DrawPixel(x, y, color);
}

void LCD_DrawHLine(uint16_t x0, uint16_t x1, uint16_t y, uint16_t color)
{
    if (x0 > x1) { uint16_t t = x0; x0 = x1; x1 = t; }
    for (uint16_t x = x0; x <= x1; x++) {
        SDL_GFX_DrawPixel(x, y, color);
    }
}

void LCD_DrawVLine(uint16_t x, uint16_t y0, uint16_t y1, uint16_t color)
{
    if (y0 > y1) { uint16_t t = y0; y0 = y1; y1 = t; }
    for (uint16_t y = y0; y <= y1; y++) {
        SDL_GFX_DrawPixel(x, y, color);
    }
}

void LCD_DrawRect(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t color)
{
    SDL_GFX_DrawRect(x0, y0, x1, y1, color);
}

void LCD_FillRect(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t color)
{
    SDL_GFX_FillRect(x0, y0, x1, y1, color);
}

void LCD_DrawProgressBar(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint8_t pct, uint16_t barColor, uint16_t bgColor)
{
    if (pct > 100) pct = 100;
    SDL_GFX_FillRect(x, y, x + w, y + h, bgColor);
    uint16_t fillW = ((w - 2) * pct) / 100;
    if (fillW > 0)
        SDL_GFX_FillRect(x + 1, y + 1, x + 1 + fillW, y + h - 1, barColor);
}

void LCD_Backlight_On(void)
{
}

void LCD_SetBrightness(uint8_t pct)
{
    (void)pct;
}

void GFX_DrawChar(int16_t x, int16_t y, char c, uint16_t color, uint16_t bg)
{
    SDL_GFX_DrawChar(x, y, c, color, bg);
}

void GFX_DrawString(int16_t x, int16_t y, const char *str, uint16_t color, uint16_t bg)
{
    SDL_GFX_DrawString(x, y, str, color, bg);
}

void GFX_DrawStringCenter(int16_t y, const char *str, uint16_t color, uint16_t bg)
{
    SDL_GFX_DrawStringCenter(y, str, color, bg);
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
    SDL_GFX_DrawString(x, y, buf, color, bg);
}

void GFX_DrawFloat(int16_t x, int16_t y, float val, uint8_t decimals, uint16_t color, uint16_t bg)
{
    char buf[16];
    snprintf(buf, sizeof(buf), "%.*f", decimals, val);
    SDL_GFX_DrawString(x, y, buf, color, bg);
}

void GFX_DrawCharScaled(int16_t x, int16_t y, char c, uint16_t color, uint16_t bg, uint8_t scale)
{
    SDL_GFX_DrawCharScaled(x, y, c, color, bg, scale);
}

void GFX_DrawStringScaled(int16_t x, int16_t y, const char *str, uint16_t color, uint16_t bg, uint8_t scale)
{
    SDL_GFX_DrawStringScaled(x, y, str, color, bg, scale);
}