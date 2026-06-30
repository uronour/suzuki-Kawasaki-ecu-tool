#include "main.h"
#include "includes.h"
#include "xpt2046.h"
#include "touch_calib.h"
#include "encoder.h"
#include "boot_screen.h"
#include "LCD_Init.h"
#include "BT_ResetScript.h"

RCC_ClocksTypeDef rccClocks;
CLOCKS mcuClocks;

static uint32_t g_lastKeepAlive = 0;
static uint32_t g_idleStart = 0;
static int16_t g_encAccum = 0;

static void Touch_Handle(void)
{
  uint8_t pen = XPT2046_Read_Pen();
  static uint8_t g_touchDown = 0;

  if (pen == 0)
  {
    if (!g_touchDown)
    {
      g_touchDown = 1;
      g_idleStart = OS_GetTimeMs();

      uint16_t tx, ty;
      TP_GetCoordinates(&tx, &ty);
      Gauge_TouchAt(tx, ty);
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
  int8_t dir = Encoder_GetDir();
  if (dir != 0)
  {
    g_encAccum += dir;
    if (abs(g_encAccum) >= 1)
    {
      g_idleStart = OS_GetTimeMs();
      if (g_encAccum >= 1) Gauge_NextPage();
      else Gauge_PrevPage();
      g_encAccum = 0;
    }
  }

  if (Encoder_GetPress())
  {
    g_idleStart = OS_GetTimeMs();
    Gauge_Press();
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

  // Reset Trigger Check
  GPIO_InitSet(LCD_BTN_PIN, MGPIO_MODE_IPU, 0);
  Delay_ms(50);
  if (GPIO_GetLevel(LCD_BTN_PIN) == 0) {
     BT_HC05_ResetScript();
  }

  XPT2046_Init();
  TP_LoadCalibration();
  Encoder_Init();

  Boot_Animation();
  Gauge_Init();
  BT_Init();
  SD_Log_Init();

  g_idleStart = OS_GetTimeMs();

  // Try non-blocking init in loop
  g_lastKeepAlive = OS_GetTimeMs();

  uint32_t lastHeartbeat = 0;

  for (;;)
  {
    // Only poll if bike was successfully initialized
    if (SDS_IsConnected()) {
      SDS_PollSensorData();
    } else {
      // Try to re-connect every 5 seconds if not connected
      if (OS_GetTimeMs() - lastHeartbeat > 5000) {
        SDS_Init();
        lastHeartbeat = OS_GetTimeMs();
        BT_SendLine("heartbeat - searching for ECU...");
      }
    }

    Gauge_Update();
    BT_Stream();
    SD_Log_Tick();
    Touch_Handle();
    Encoder_Handle();

    if (SDS_IsConnected() && (OS_GetTimeMs() - g_lastKeepAlive >= SDS_KEEPALIVE_MS))
    {
      SDS_SendKeepAlive();
      g_lastKeepAlive = OS_GetTimeMs();
    }
  }
}
