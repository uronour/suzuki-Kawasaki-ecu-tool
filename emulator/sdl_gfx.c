#include "sdl_gfx.h"
#include "font_8x13.h"
#include <string.h>
#include <stdlib.h>

static SDL_Window *g_window = NULL;
static SDL_Renderer *g_renderer = NULL;
static SDL_Texture *g_texture = NULL;
static uint32_t *g_framebuffer = NULL;
static uint32_t g_startTime = 0;

void SDL_GFX_Init(void)
{
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        exit(1);
    }

    g_window = SDL_CreateWindow("GSX-R1000 Gauge Simulator",
                                SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                LCD_WIDTH * 2, LCD_HEIGHT * 2,
                                SDL_WINDOW_RESIZABLE);
    if (!g_window) {
        SDL_Log("SDL_CreateWindow failed: %s", SDL_GetError());
        exit(1);
    }

    g_renderer = SDL_CreateRenderer(g_window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!g_renderer) {
        SDL_Log("SDL_CreateRenderer failed: %s", SDL_GetError());
        exit(1);
    }

    g_texture = SDL_CreateTexture(g_renderer, SDL_PIXELFORMAT_ARGB8888,
                                  SDL_TEXTUREACCESS_STREAMING,
                                  LCD_WIDTH, LCD_HEIGHT);
    if (!g_texture) {
        SDL_Log("SDL_CreateTexture failed: %s", SDL_GetError());
        exit(1);
    }

    g_framebuffer = (uint32_t *)malloc(LCD_WIDTH * LCD_HEIGHT * sizeof(uint32_t));
    if (!g_framebuffer) {
        SDL_Log("Framebuffer allocation failed");
        exit(1);
    }

    SDL_RenderSetLogicalSize(g_renderer, LCD_WIDTH, LCD_HEIGHT);
    g_startTime = SDL_GetTicks();
}

void SDL_GFX_Quit(void)
{
    if (g_framebuffer) free(g_framebuffer);
    if (g_texture) SDL_DestroyTexture(g_texture);
    if (g_renderer) SDL_DestroyRenderer(g_renderer);
    if (g_window) SDL_DestroyWindow(g_window);
    SDL_Quit();
}

static inline void draw_pixel_fb(int16_t x, int16_t y, uint32_t color)
{
    if (x >= 0 && x < LCD_WIDTH && y >= 0 && y < LCD_HEIGHT) {
        g_framebuffer[y * LCD_WIDTH + x] = color;
    }
}

void SDL_GFX_Clear(uint16_t color)
{
    SDL_Color col = RGB565_to_SDL(color);
    uint32_t argb = (col.a << 24) | (col.r << 16) | (col.g << 8) | col.b;
    for (int i = 0; i < LCD_WIDTH * LCD_HEIGHT; i++) {
        g_framebuffer[i] = argb;
    }
}

void SDL_GFX_Present(void)
{
    SDL_UpdateTexture(g_texture, NULL, g_framebuffer, LCD_WIDTH * sizeof(uint32_t));
    SDL_RenderClear(g_renderer);
    SDL_RenderCopy(g_renderer, g_texture, NULL, NULL);
    SDL_RenderPresent(g_renderer);
}

void SDL_GFX_DrawPixel(int16_t x, int16_t y, uint16_t color)
{
    SDL_Color col = RGB565_to_SDL(color);
    uint32_t argb = (col.a << 24) | (col.r << 16) | (col.g << 8) | col.b;
    draw_pixel_fb(x, y, argb);
}

void SDL_GFX_FillRect(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color)
{
    if (x0 > x1) { int16_t t = x0; x0 = x1; x1 = t; }
    if (y0 > y1) { int16_t t = y0; y0 = y1; y1 = t; }
    if (x1 >= LCD_WIDTH) x1 = LCD_WIDTH - 1;
    if (y1 >= LCD_HEIGHT) y1 = LCD_HEIGHT - 1;
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;

    SDL_Color col = RGB565_to_SDL(color);
    uint32_t argb = (col.a << 24) | (col.r << 16) | (col.g << 8) | col.b;

    for (int16_t y = y0; y <= y1; y++) {
        for (int16_t x = x0; x <= x1; x++) {
            g_framebuffer[y * LCD_WIDTH + x] = argb;
        }
    }
}

void SDL_GFX_DrawRect(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color)
{
    SDL_GFX_DrawLine(x0, y0, x1, y0, color);
    SDL_GFX_DrawLine(x0, y1, x1, y1, color);
    SDL_GFX_DrawLine(x0, y0, x0, y1, color);
    SDL_GFX_DrawLine(x1, y0, x1, y1, color);
}

void SDL_GFX_DrawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color)
{
    int16_t dx = abs(x1 - x0);
    int16_t dy = abs(y1 - y0);
    int16_t sx = (x0 < x1) ? 1 : -1;
    int16_t sy = (y0 < y1) ? 1 : -1;
    int16_t err = dx - dy;

    while (1) {
        SDL_GFX_DrawPixel(x0, y0, color);
        if (x0 == x1 && y0 == y1) break;
        int16_t e2 = 2 * err;
        if (e2 > -dy) { err -= dy; x0 += sx; }
        if (e2 < dx) { err += dx; y0 += sy; }
    }
}

void SDL_GFX_DrawCircle(int16_t x0, int16_t y0, int16_t r, uint16_t color)
{
    int16_t x = r, y = 0;
    int16_t err = 0;

    while (x >= y) {
        SDL_GFX_DrawPixel(x0 + x, y0 + y, color);
        SDL_GFX_DrawPixel(x0 + y, y0 + x, color);
        SDL_GFX_DrawPixel(x0 - y, y0 + x, color);
        SDL_GFX_DrawPixel(x0 - x, y0 + y, color);
        SDL_GFX_DrawPixel(x0 - x, y0 - y, color);
        SDL_GFX_DrawPixel(x0 - y, y0 - x, color);
        SDL_GFX_DrawPixel(x0 + y, y0 - x, color);
        SDL_GFX_DrawPixel(x0 + x, y0 - y, color);

        y++;
        err += 1 + 2*y;
        if (2*(err-x) + 1 > 0) {
            x--;
            err += 1 - 2*x;
        }
    }
}

void SDL_GFX_FillCircle(int16_t x0, int16_t y0, int16_t r, uint16_t color)
{
    for (int16_t y = -r; y <= r; y++) {
        for (int16_t x = -r; x <= r; x++) {
            if (x*x + y*y <= r*r) {
                SDL_GFX_DrawPixel(x0 + x, y0 + y, color);
            }
        }
    }
}

void SDL_GFX_DrawChar(int16_t x, int16_t y, char c, uint16_t color, uint16_t bg)
{
    if (c < 32 || c > 126) c = 32;
    uint8_t idx = c - 32;
    SDL_Color fg = RGB565_to_SDL(color);
    uint32_t fg_argb = (fg.a << 24) | (fg.r << 16) | (fg.g << 8) | fg.b;
    SDL_Color bgcol = RGB565_to_SDL(bg);
    uint32_t bg_argb = (bgcol.a << 24) | (bgcol.r << 16) | (bgcol.g << 8) | bgcol.b;

    for (uint8_t row = 0; row < FONT_H; row++) {
        uint8_t bits = font_8x13[idx][row];
        for (uint8_t col = 0; col < FONT_W; col++) {
            uint32_t pixel_color = (bits & (0x80 >> col)) ? fg_argb : bg_argb;
            draw_pixel_fb(x + col, y + row, pixel_color);
        }
    }
}

void SDL_GFX_DrawString(int16_t x, int16_t y, const char *str, uint16_t color, uint16_t bg)
{
    while (*str) {
        SDL_GFX_DrawChar(x, y, *str++, color, bg);
        x += FONT_STEP;
    }
}

void SDL_GFX_DrawStringCenter(int16_t y, const char *str, uint16_t color, uint16_t bg)
{
    int16_t len = 0;
    const char *p = str;
    while (*p++) len++;
    int16_t x = (LCD_WIDTH - len * FONT_STEP) / 2;
    if (x < 0) x = 0;
    SDL_GFX_DrawString(x, y, str, color, bg);
}

void SDL_GFX_DrawCharScaled(int16_t x, int16_t y, char c, uint16_t color, uint16_t bg, uint8_t scale)
{
    if (c < 32 || c > 126) c = 32;
    uint8_t idx = c - 32;
    SDL_Color fg = RGB565_to_SDL(color);
    uint32_t fg_argb = (fg.a << 24) | (fg.r << 16) | (fg.g << 8) | fg.b;
    SDL_Color bgcol = RGB565_to_SDL(bg);
    uint32_t bg_argb = (bgcol.a << 24) | (bgcol.r << 16) | (bgcol.g << 8) | bgcol.b;

    for (uint8_t row = 0; row < FONT_H; row++) {
        uint8_t bits = font_8x13[idx][row];
        for (uint8_t col = 0; col < FONT_W; col++) {
            if (bits & (0x80 >> col)) {
                for (uint8_t dy = 0; dy < scale; dy++) {
                    for (uint8_t dx = 0; dx < scale; dx++) {
                        draw_pixel_fb(x + col * scale + dx, y + row * scale + dy, fg_argb);
                    }
                }
            } else {
                for (uint8_t dy = 0; dy < scale; dy++) {
                    for (uint8_t dx = 0; dx < scale; dx++) {
                        draw_pixel_fb(x + col * scale + dx, y + row * scale + dy, bg_argb);
                    }
                }
            }
        }
    }
}

void SDL_GFX_DrawStringScaled(int16_t x, int16_t y, const char *str, uint16_t color, uint16_t bg, uint8_t scale)
{
    while (*str) {
        SDL_GFX_DrawCharScaled(x, y, *str++, color, bg, scale);
        x += FONT_STEP * scale;
    }
}