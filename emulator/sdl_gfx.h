#ifndef _SDL_GFX_H_
#define _SDL_GFX_H_

#include <stdint.h>
#include <SDL2/SDL.h>

#define LCD_WIDTH  480
#define LCD_HEIGHT 320
#define FONT_W 8
#define FONT_H 13
#define FONT_STEP 10

#define COLOR_BLACK     0x0000
#define COLOR_WHITE     0xFFFF
#define COLOR_RED       0xF800
#define COLOR_GREEN     0x07E0
#define COLOR_BLUE      0x001F
#define COLOR_GRAY      0x8410
#define COLOR_YELLOW    0xFFE0
#define COLOR_CYAN      0x07FF
#define COLOR_MAGENTA   0xF81F
#define COLOR_ORANGE    0xFC00

static inline uint16_t RGB565(uint8_t r, uint8_t g, uint8_t b)
{
    return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
}

static inline SDL_Color RGB565_to_SDL(uint16_t c)
{
    SDL_Color col;
    col.r = ((c >> 11) & 0x1F) << 3;
    col.g = ((c >> 5) & 0x3F) << 2;
    col.b = (c & 0x1F) << 3;
    col.a = 255;
    return col;
}

void SDL_GFX_Init(void);
void SDL_GFX_Quit(void);
uint32_t SDL_GFX_GetStartTime(void);
void SDL_GFX_Clear(uint16_t color);
void SDL_GFX_Present(void);
void SDL_GFX_DrawPixel(int16_t x, int16_t y, uint16_t color);
void SDL_GFX_FillRect(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color);
void SDL_GFX_DrawRect(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color);
void SDL_GFX_DrawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color);
void SDL_GFX_DrawCircle(int16_t x0, int16_t y0, int16_t r, uint16_t color);
void SDL_GFX_FillCircle(int16_t x0, int16_t y0, int16_t r, uint16_t color);
void SDL_GFX_DrawChar(int16_t x, int16_t y, char c, uint16_t color, uint16_t bg);
void SDL_GFX_DrawString(int16_t x, int16_t y, const char *str, uint16_t color, uint16_t bg);
void SDL_GFX_DrawStringCenter(int16_t y, const char *str, uint16_t color, uint16_t bg);
void SDL_GFX_DrawCharScaled(int16_t x, int16_t y, char c, uint16_t color, uint16_t bg, uint8_t scale);
void SDL_GFX_DrawStringScaled(int16_t x, int16_t y, const char *str, uint16_t color, uint16_t bg, uint8_t scale);

#endif