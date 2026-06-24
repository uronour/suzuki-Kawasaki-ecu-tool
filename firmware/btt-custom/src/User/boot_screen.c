#include <stdio.h>
#include <string.h>
#include "boot_screen.h"
#include "ili9488_gfx.h"
#include "gauge_dial.h"
#include "os_timer.h"
#include "delay.h"
#include "lcd.h"

static void DrawGSXRLogoCentered(GaugeDial *d)
{
  int16_t x = d->cx - 72;
  int16_t y = d->cy - 45;
  GFX_DrawStringScaled(x, y, "GSX-R", RGB565(255, 40, 0), COLOR_BLACK, 3);
}

void Boot_Animation(void)
{
  GaugeDial bootTach = { 240, 160, 120, 135, 45, 0, 14000, 8 };
  DIAL_InitTable();

  uint32_t t0 = OS_GetTimeMs();
  uint32_t lastFrame = t0;
  uint32_t prevRpm = 0xFFFFFFFF;

  LCD_Fill(0, 0, LCD_WIDTH - 1, LCD_HEIGHT - 1, COLOR_BLACK);

  GFX_DrawStringScaled(80, 30, "Suzuki ECU Tool", RGB565(0, 180, 255), COLOR_BLACK, 3);
  GFX_DrawStringScaled(110, 75, "2004 GSX-R1000", RGB565(150, 150, 150), COLOR_BLACK, 2);

  DIAL_DrawArcSweep(&bootTach, 135, 45, RGB565(0, 180, 0));
  DIAL_DrawArcSweep(&bootTach, DIAL_AngleForValue(&bootTach, 10000), DIAL_AngleForValue(&bootTach, 12000), RGB565(200, 180, 0));
  DIAL_DrawArcSweep(&bootTach, DIAL_AngleForValue(&bootTach, 12000), 45, RGB565(200, 0, 0));
  DIAL_DrawTicks(&bootTach, 500, 2000, RGB565(120, 120, 120), RGB565(180, 180, 180));
  DIAL_DrawCenterHub(&bootTach, 12, RGB565(60, 60, 60));
  DrawGSXRLogoCentered(&bootTach);

  while (OS_GetTimeMs() - t0 < 1500)
  {
    uint32_t now = OS_GetTimeMs();
    if (now - lastFrame < 16) { Delay_ms(1); continue; }
    lastFrame = now;

    uint32_t progress = now - t0;
    uint32_t sweepRpm;
    if (progress < 600)
    {
      sweepRpm = progress * 14000 / 600;
    }
    else if (progress < 1200)
    {
      sweepRpm = 14000 - (progress - 600) * 14000 / 600;
    }
    else
    {
      sweepRpm = 0;
    }

    if (sweepRpm != prevRpm)
    {
      DIAL_DrawNeedle(&bootTach, sweepRpm, RGB565(255, 80, 0), COLOR_BLACK, prevRpm);
      prevRpm = sweepRpm;

      char rpmStr[16];
      snprintf(rpmStr, sizeof(rpmStr), "%u RPM", (unsigned)sweepRpm);
      int16_t w = strlen(rpmStr) * 8 * 2;
      LCD_FillRect(bootTach.cx - w/2 - 4, bootTach.cy + 30, bootTach.cx + w/2 + 4, bootTach.cy + 30 + 13*2 + 4, COLOR_BLACK);
      GFX_DrawStringScaled(bootTach.cx - w/2, bootTach.cy + 35, rpmStr, RGB565(255, 255, 255), COLOR_BLACK, 2);
    }
  }

  if (prevRpm != 0)
  {
    DIAL_DrawNeedle(&bootTach, 0, RGB565(255, 80, 0), COLOR_BLACK, prevRpm);
    LCD_FillRect(bootTach.cx - 40, bootTach.cy + 30, bootTach.cx + 40, bootTach.cy + 30 + 13*2 + 4, COLOR_BLACK);
    GFX_DrawStringScaled(bootTach.cx - 30, bootTach.cy + 35, "0 RPM", RGB565(200, 200, 200), COLOR_BLACK, 2);
  }

  GFX_DrawStringScaled(140, 270, "Made by Uronour", RGB565(100, 100, 100), COLOR_BLACK, 1);
  Delay_ms(300);
}
