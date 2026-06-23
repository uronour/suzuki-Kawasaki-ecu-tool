#include "main.h"
#include "includes.h"
#include "xpt2046.h"
#include "encoder.h"

RCC_ClocksTypeDef rccClocks;
CLOCKS mcuClocks;

static uint32_t g_lastKeepAlive = 0;
static uint32_t g_lastTouch = 0;
static uint8_t g_touchDown = 0;
static int16_t g_encAccum = 0;

static void Touch_Handle(void)
{
  if (OS_GetTimeMs() - g_lastTouch < 300)
    return;

  uint8_t pen = XPT2046_Read_Pen();
  if (pen == 0)
  {
    if (!g_touchDown)
    {
      g_touchDown = 1;
      g_lastTouch = OS_GetTimeMs();

      uint16_t tx = XPT2046_Repeated_Compare_AD(0xD0);
      if (tx > 2047)
        Gauge_NextPage();
      else
        Gauge_PrevPage();
    }
  }
  else
  {
    g_touchDown = 0;
  }
}

static void Encoder_Handle(void)
{
  Encoder_Poll();

  g_encAccum += Encoder_GetDir();
  if (g_encAccum >= 2)
  {
    g_encAccum = 0;
    Gauge_NextPage();
  }
  else if (g_encAccum <= -2)
  {
    g_encAccum = 0;
    Gauge_PrevPage();
  }

  if (Encoder_GetPress())
  {
    Gauge_NextPage();
  }
}

int main(void)
{
  SystemClockInit();
  SCB->VTOR = VECT_TAB_FLASH;

  RCC_GetClocksFreq(&rccClocks);
  mcuClocks.rccClocks = rccClocks;
  mcuClocks.PCLK1_Timer_Frequency = (rccClocks.PCLK1_Frequency * 2);
  mcuClocks.PCLK2_Timer_Frequency = (rccClocks.PCLK2_Frequency * 2);

  Delay_init();
  OS_InitTimerMs();
  LCD_Init();
  XPT2046_Init();
  Encoder_Init();

  Gauge_Init();
  BT_Init();

  SDS_Init();

  g_lastKeepAlive = OS_GetTimeMs();

  for (;;)
  {
    SDS_PollSensorData();
    Gauge_Update();
    BT_Stream();
    Touch_Handle();
    Encoder_Handle();

    if (OS_GetTimeMs() - g_lastKeepAlive >= SDS_KEEPALIVE_MS)
    {
      SDS_SendKeepAlive();
      g_lastKeepAlive = OS_GetTimeMs();
    }
  }
}