#include "encoder.h"
#include "GPIO_Init.h"
#include "os_timer.h"
#include <stdio.h>

#define ENC_A    PA8
#define ENC_B    PC9
#define ENC_BTN  PC8

static uint8_t g_lastState = 0;
static int8_t g_dir = 0;
static uint32_t g_lastEncTime = 0;
static uint32_t g_lastBtnTime = 0;
static uint8_t g_press = 0;
static uint8_t g_longPress = 0;
static uint8_t g_lastBtnState = 1;

#define ENC_DEBOUNCE_MS  3
#define ENC_LONGPRESS_MS 1500

static const int8_t encTable[16] = {
  0, 1, -1, 0,
  -1, 0, 0, 1,
  1, 0, 0, -1,
  0, -1, 1, 0
};

void Encoder_Init(void)
{
  GPIO_InitSet(ENC_A, MGPIO_MODE_IPU, 0);
  GPIO_InitSet(ENC_B, MGPIO_MODE_IPU, 0);
  GPIO_InitSet(ENC_BTN, MGPIO_MODE_IPU, 0);
  g_lastState = (GPIO_GetLevel(ENC_A) << 1) | GPIO_GetLevel(ENC_B);
  g_lastEncTime = OS_GetTimeMs();
  g_lastBtnTime = OS_GetTimeMs();
  g_lastBtnState = GPIO_GetLevel(ENC_BTN);
  printf("[ENC] Initialized: A=PA8, B=PC9, BTN=PC8\n");
}

int8_t Encoder_GetDir(void)
{
  int8_t d = g_dir;
  g_dir = 0;
  return d;
}

uint8_t Encoder_GetPress(void)
{
  uint8_t p = g_press;
  g_press = 0;
  return p;
}

uint8_t Encoder_GetLongPress(void)
{
  uint8_t p = g_longPress;
  g_longPress = 0;
  return p;
}

void Encoder_Poll(void)
{
  uint32_t now = OS_GetTimeMs();

  // Read encoder pins
  uint8_t a = GPIO_GetLevel(ENC_A);
  uint8_t b = GPIO_GetLevel(ENC_B);
  uint8_t state = (a << 1) | b;

  if (state != g_lastState && (now - g_lastEncTime) >= ENC_DEBOUNCE_MS)
  {
    uint8_t idx = (g_lastState << 2) | state;
    int8_t delta = encTable[idx];
    if (delta != 0)
    {
      g_dir += delta;
    }
    g_lastEncTime = now;
    g_lastState = state;
  }

  // Button polling with debouncing and long-press detection
  uint8_t btn = GPIO_GetLevel(ENC_BTN);
  if (btn != g_lastBtnState && (now - g_lastBtnTime) >= ENC_DEBOUNCE_MS)
  {
    if (btn == 0)  // Button pressed (active low)
    {
      g_press = 1;
      g_lastBtnTime = now;
    }
    g_lastBtnState = btn;
  }

  // Long-press: button held down for ENC_LONGPRESS_MS
  if (g_lastBtnState == 0 && (now - g_lastBtnTime) >= ENC_LONGPRESS_MS && !g_longPress)
  {
    g_longPress = 1;
    g_press = 0;   // cancel short press
  }
}
