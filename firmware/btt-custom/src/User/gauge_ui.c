#include <stdio.h>
#include <string.h>
#include "gauge_ui.h"
#include "gauge_dial.h"
#include "ili9488_gfx.h"
#include "SDSProtocol.h"
#include "kline_task.h"
#include "bt_stream.h"
#include "sd_log.h"
#include "font_8x13.h"
#include "os_timer.h"

#define TACH_CX      140
#define TACH_CY      200
#define TACH_R       130
#define TACH_START   135
#define TACH_END     45
#define TACH_ARC_W   8
#define TACH_MAX     14000
#define TACH_REDLINE 12000
#define TACH_YLIMIT  10000

#define DIG_X        310
#define DIG_LABEL_Y  95
#define DIG_VAL_Y    120
#define DIG_STEP     65
#define DIG_SCALE    3

#define PAGE_FOOTER_Y (LCD_HEIGHT - FONT_H - 4)

#define SPEEDO_CX    340
#define SPEEDO_CY    200
#define SPEEDO_R     130
#define SPEEDO_START 135
#define SPEEDO_END   45
#define SPEEDO_ARC_W 8
#define SPEEDO_MAX   300

static GaugePage g_currentPage = GAUGE_PAGE_DASHBOARD;
static DashStyle g_dashStyle = DASH_STYLE_OEM;
static uint32_t g_lastUpdate = 0;
static uint32_t g_prevNeedleVal = 0xFFFFFFFF;
static uint32_t g_prevSpeedVal = 0xFFFFFFFF;
static uint32_t g_prevDig[4] = {0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF};
static uint8_t g_detailMode = 0;
static char g_prevRpmStr[16] = {0};
static uint32_t g_prevDetailVals[8] = {0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF};
static uint8_t g_detailDrawn = 0;
static uint8_t g_prevGear = 0xFF;

static uint8_t g_pageDirty = 1;
static const char *gearStr[] = { "N", "1", "2", "3", "4", "5", "6" };
static uint32_t g_prevSensors[4] = {0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF};
static uint16_t g_prevDtcCount = 0xFFFF;
static uint8_t g_prevSettings[4] = {0xFF, 0xFF, 0xFF, 0xFF};
static uint8_t g_aboutDrawn = 0;

static GaugeDial g_tach = {
  .cx = TACH_CX, .cy = TACH_CY,
  .radius = TACH_R,
  .startAngle = TACH_START, .endAngle = TACH_END,
  .minVal = 0, .maxVal = TACH_MAX,
  .arcWidth = TACH_ARC_W
};

static GaugeDial g_speedo = {
  .cx = SPEEDO_CX, .cy = SPEEDO_CY,
  .radius = SPEEDO_R,
  .startAngle = SPEEDO_START, .endAngle = SPEEDO_END,
  .minVal = 0, .maxVal = SPEEDO_MAX,
  .arcWidth = SPEEDO_ARC_W
};

static void DigLabel(int idx, const char *label)
{
  int16_t y = DIG_LABEL_Y + idx * DIG_STEP;
  GFX_DrawString(DIG_X, y, label, COLOR_WHITE, COLOR_BLACK);
}

static void DigValue(int idx, const char *text, uint16_t color)
{
  int16_t y = DIG_VAL_Y + idx * DIG_STEP;
  int16_t w = strlen(text) * FONT_STEP * DIG_SCALE;
  int16_t h = FONT_H * DIG_SCALE;
  LCD_FillRect(DIG_X - 4, y - 4, DIG_X + w + 4, y + h + 4, COLOR_BLACK);
  GFX_DrawStringScaled(DIG_X, y, text, color, COLOR_BLACK, DIG_SCALE);
}

static void DigValueU32(int idx, uint32_t val, const char *unit, uint16_t color)
{
  char buf[24];
  snprintf(buf, sizeof(buf), "%lu %s", (unsigned long)val, unit ? unit : "");
  DigValue(idx, buf, color);
}

static void DrawTachoFace(void)
{
  DIAL_DrawArcSweep(&g_tach, TACH_START, DIAL_AngleForValue(&g_tach, TACH_YLIMIT), RGB565(0, 180, 0));
  DIAL_DrawArcSweep(&g_tach, DIAL_AngleForValue(&g_tach, TACH_YLIMIT), DIAL_AngleForValue(&g_tach, TACH_REDLINE), RGB565(200, 180, 0));
  DIAL_DrawArcSweep(&g_tach, DIAL_AngleForValue(&g_tach, TACH_REDLINE), TACH_END, RGB565(200, 0, 0));
  DIAL_DrawTicks(&g_tach, 500, 2000, RGB565(180, 180, 180), RGB565(220, 220, 220));
  DIAL_DrawCenterHub(&g_tach, 12, RGB565(80, 80, 80));
  GFX_DrawStringScaled(g_tach.cx - 36, g_tach.cy - 9, "GSX-R", RGB565(255, 40, 0), RGB565(30, 30, 30), 2);
}

static void DrawSpeedoFace(void)
{
  DIAL_DrawArcSweep(&g_speedo, SPEEDO_START, DIAL_AngleForValue(&g_speedo, 100), RGB565(0, 180, 0));
  DIAL_DrawArcSweep(&g_speedo, DIAL_AngleForValue(&g_speedo, 100), DIAL_AngleForValue(&g_speedo, 180), RGB565(200, 180, 0));
  DIAL_DrawArcSweep(&g_speedo, DIAL_AngleForValue(&g_speedo, 180), SPEEDO_END, RGB565(200, 0, 0));
  DIAL_DrawTicks(&g_speedo, 20, 40, RGB565(180, 180, 180), RGB565(220, 220, 220));
  DIAL_DrawCenterHub(&g_speedo, 12, RGB565(80, 80, 80));
  GFX_DrawStringScaled(g_speedo.cx - 30, g_speedo.cy - 9, "MPH", RGB565(255, 40, 0), RGB565(30, 30, 30), 2);
}

void Gauge_Init(void)
{
  DIAL_InitTable();
  LCD_Fill(0, 0, LCD_WIDTH - 1, LCD_HEIGHT - 1, RGB565(30, 30, 30));
  g_currentPage = GAUGE_PAGE_DASHBOARD;
  g_dashStyle = DASH_STYLE_OEM;
  g_prevNeedleVal = 0xFFFFFFFF;
  for (int i = 0; i < 4; i++) g_prevDig[i] = 0xFFFFFFFF;

  DrawTachoFace();
  DrawSpeedoFace();
  DigLabel(0, "Coolant");
  DigLabel(1, "Speed");
  DigLabel(2, "Battery");
  DigLabel(3, "Gear");

  char tmp[8];
  snprintf(tmp, sizeof(tmp), "%d/%d", g_currentPage + 1, GAUGE_PAGE_COUNT);
  GFX_DrawStringCenter(PAGE_FOOTER_Y, tmp, COLOR_GRAY, RGB565(30, 30, 30));
}

void Gauge_Update(void)
{
  if (OS_GetTimeMs() - g_lastUpdate < 16)
    return;
  g_lastUpdate = OS_GetTimeMs();

  switch (g_currentPage)
  {
    case GAUGE_PAGE_DASHBOARD:
    {
      uint32_t rpm = g_sdsData.rpm;
      if (rpm > TACH_MAX) rpm = TACH_MAX;
      DIAL_DrawNeedle(&g_tach, rpm, RGB565(255, 80, 0), RGB565(30, 30, 30), g_prevNeedleVal);
      g_prevNeedleVal = rpm;

      uint32_t speedMph = (uint32_t)((unsigned long)g_sdsData.speed * 621371UL / 1000000UL);
      if (speedMph > SPEEDO_MAX) speedMph = SPEEDO_MAX;
      DIAL_DrawNeedle(&g_speedo, speedMph, RGB565(255, 80, 0), RGB565(30, 30, 30), g_prevSpeedVal);
      g_prevSpeedVal = speedMph;

      char rpmStr[16];
      snprintf(rpmStr, sizeof(rpmStr), "%u RPM", g_sdsData.rpm);
      if (strcmp(rpmStr, g_prevRpmStr) != 0)
      {
        GFX_DrawStringScaled(TACH_CX - 50, TACH_CY + 35, rpmStr, COLOR_WHITE, RGB565(30, 30, 30), 2);
        strcpy(g_prevRpmStr, rpmStr);
      }

      // Gear indicator inside RPM gauge
      uint8_t gear = g_sdsData.gearPos;
      if (gear > 6) gear = 0;
      if (gear != g_prevGear)
      {
        g_prevGear = gear;
        int16_t gearX = TACH_CX;
        int16_t gearY = TACH_CY - 50;
        int16_t gearW = 48;
        int16_t gearH = 36;
        LCD_FillRect(gearX - gearW/2 - 4, gearY - gearH/2 - 4, gearX + gearW/2 + 4, gearY + gearH/2 + 4, RGB565(20, 20, 20));
        GFX_DrawStringScaled(gearX - 12, gearY - 18, gearStr[gear], 
            (gear == 0) ? RGB565(100, 200, 100) : RGB565(255, 200, 0), 
            RGB565(20, 20, 20), 4);
      }

      uint32_t dig[4] = { g_sdsData.coolantTemp, speedMph, g_sdsData.batteryVolt, g_sdsData.gearPos };
      const char *units[4] = { "C", "mph", "V", "" };
      for (int i = 0; i < 4; i++)
      {
        if (dig[i] != g_prevDig[i])
        {
          g_prevDig[i] = dig[i];
          DigValueU32(i, dig[i], units[i], COLOR_WHITE);
        }
      }

      if (g_detailMode)
      {
        if (!g_detailDrawn)
        {
          LCD_FillRect(10, 160, 290, 310, RGB565(20, 20, 20));
          GFX_DrawStringScaled(15, 165, "MAP", RGB565(100, 100, 100), RGB565(20, 20, 20), 1);
          GFX_DrawStringScaled(15, 185, "IAT", RGB565(100, 100, 100), RGB565(20, 20, 20), 1);
          GFX_DrawStringScaled(15, 205, "TPS", RGB565(100, 100, 100), RGB565(20, 20, 20), 1);
          GFX_DrawStringScaled(15, 225, "O2", RGB565(100, 100, 100), RGB565(20, 20, 20), 1);
          GFX_DrawStringScaled(15, 245, "Inject", RGB565(100, 100, 100), RGB565(20, 20, 20), 1);
          GFX_DrawStringScaled(15, 265, "Ign", RGB565(100, 100, 100), RGB565(20, 20, 20), 1);
          GFX_DrawStringScaled(15, 285, "IAC", RGB565(100, 100, 100), RGB565(20, 20, 20), 1);
          g_detailDrawn = 1;
        }

        uint32_t detailVals[8] = { g_sdsData.mapKpa, g_sdsData.intakeAirTemp, g_sdsData.throttlePos, g_sdsData.o2Sensor,
                                   g_sdsData.injectorPulse, g_sdsData.ignitionTiming, g_sdsData.iacStep, 0 };
        const char *detailUnits[8] = { "kPa", "C", "%", "", "ms", "deg", "", "" };
        int16_t detailY[8] = { 165, 185, 205, 225, 245, 265, 285, 0 };
        for (int i = 0; i < 7; i++)
        {
          if (detailVals[i] != g_prevDetailVals[i])
          {
            g_prevDetailVals[i] = detailVals[i];
            char buf[16];
            snprintf(buf, sizeof(buf), "%lu %s", (unsigned long)detailVals[i], detailUnits[i]);
            int16_t w = strlen(buf) * FONT_STEP;
            int16_t h = FONT_H;
            LCD_FillRect(80, detailY[i], 80 + w, detailY[i] + h, RGB565(20, 20, 20));
            GFX_DrawStringScaled(80, detailY[i], buf, COLOR_WHITE, RGB565(20, 20, 20), 1);
          }
        }
      }
      else
      {
        g_detailDrawn = 0;
        for (int i = 0; i < 8; i++) g_prevDetailVals[i] = 0xFFFFFFFF;
      }
      break;
    }

    case GAUGE_PAGE_SENSORS:
    {
      if (g_pageDirty)
      {
        LCD_Fill(0, 0, LCD_WIDTH - 1, LCD_HEIGHT - 21, RGB565(30, 30, 30));
        GFX_DrawStringScaled(30, 80, "MAP", COLOR_GRAY, RGB565(30, 30, 30), 2);
        GFX_DrawStringScaled(30, 80 + DIG_STEP, "O2", COLOR_GRAY, RGB565(30, 30, 30), 2);
        GFX_DrawStringScaled(30, 80 + DIG_STEP * 2, "Ign Timing", COLOR_GRAY, RGB565(30, 30, 30), 2);
        GFX_DrawStringScaled(30, 80 + DIG_STEP * 3, "IAC Steps", COLOR_GRAY, RGB565(30, 30, 30), 2);
        g_pageDirty = 0;
        g_prevSensors[0] = g_sdsData.mapKpa;
        g_prevSensors[1] = g_sdsData.o2Sensor;
        g_prevSensors[2] = g_sdsData.ignitionTiming;
        g_prevSensors[3] = g_sdsData.iacStep;
      }
      else
      {
        uint32_t sensors[4] = { g_sdsData.mapKpa, g_sdsData.o2Sensor, g_sdsData.ignitionTiming, g_sdsData.iacStep };
        const char *units[4] = { "kPa", "", "deg", "" };
        for (int i = 0; i < 4; i++)
        {
          if (sensors[i] != g_prevSensors[i])
          {
            g_prevSensors[i] = sensors[i];
            DigValueU32(i, sensors[i], units[i], COLOR_WHITE);
          }
        }
      }
      break;
    }

    case GAUGE_PAGE_DTC:
    {
      uint8_t dtcBuf[32];
      uint16_t count = SDS_GetDTCs(dtcBuf, sizeof(dtcBuf));

      if (g_pageDirty || count != g_prevDtcCount)
      {
        if (g_pageDirty)
        {
          LCD_Fill(0, 0, LCD_WIDTH - 1, LCD_HEIGHT - 21, RGB565(30, 30, 30));
        }
        else
        {
          LCD_FillRect(30, 80, 250, 130, RGB565(30, 30, 30));
          for (int i = 0; i < 6; i++)
            LCD_FillRect(30, 130 + i * 30, 250, 130 + (i + 1) * 30, RGB565(30, 30, 30));
        }

        char buf[32];
        snprintf(buf, sizeof(buf), "DTC Count: %u", count);
        GFX_DrawStringScaled(30, 80, buf, COLOR_WHITE, RGB565(30, 30, 30), 2);
        if (count > 0)
        {
          for (uint16_t i = 0; i < count && i < 6; i++)
          {
            snprintf(buf, sizeof(buf), "  %02X", dtcBuf[i]);
            GFX_DrawStringScaled(30, 130 + i * 30, buf, COLOR_RED, RGB565(30, 30, 30), 2);
          }
        }
        g_prevDtcCount = count;
        g_pageDirty = 0;
      }
      break;
    }

    case GAUGE_PAGE_SETTINGS:
    {
      uint8_t settings[4] = {
        KLine_IsConnected() ? 1 : 0,
        BT_IsConnected() ? 1 : 0,
        SD_Log_IsMounted() ? 1 : 0,
        SD_Log_IsActive() ? 1 : 0
      };
      const char *labels[4] = { "K-Line:", "BT:", "SD:", "Log:" };
      const char *values[4][2] = {
        { "NC", "OK" },
        { "NC", "OK" },
        { "NC", "OK" },
        { "OFF", "ON" }
      };
      const uint16_t colors[4][2] = {
        { COLOR_RED, COLOR_GREEN },
        { COLOR_RED, COLOR_GREEN },
        { COLOR_RED, COLOR_GREEN },
        { COLOR_GRAY, COLOR_GREEN }
      };

      if (g_pageDirty)
      {
        LCD_Fill(0, 0, LCD_WIDTH - 1, LCD_HEIGHT - 21, RGB565(30, 30, 30));
        for (int i = 0; i < 4; i++)
        {
          GFX_DrawStringScaled(30, 80 + i * DIG_STEP, labels[i], COLOR_GRAY, RGB565(30, 30, 30), 2);
          DigValue(i, values[i][settings[i]], colors[i][settings[i]]);
        }
        for (int i = 0; i < 4; i++) g_prevSettings[i] = settings[i];
        g_pageDirty = 0;
      }
      else
      {
        for (int i = 0; i < 4; i++)
        {
          if (settings[i] != g_prevSettings[i])
          {
            g_prevSettings[i] = settings[i];
            DigValue(i, values[i][settings[i]], colors[i][settings[i]]);
          }
        }
      }
      break;
    }

    case GAUGE_PAGE_ABOUT:
    {
      if (g_pageDirty || !g_aboutDrawn)
      {
        LCD_Fill(0, 0, LCD_WIDTH - 1, LCD_HEIGHT - 21, RGB565(20, 20, 20));
        
        int16_t lineY = 20;
        
        GFX_DrawStringCenterScaled(lineY, "Suzuki ECU Tool", RGB565(0, 180, 255), RGB565(20, 20, 20), 3);
        lineY += 45;
        GFX_DrawStringCenterScaled(lineY, "2004 GSX-R1000", RGB565(180, 180, 180), RGB565(20, 20, 20), 2);
        lineY += 40;
        GFX_DrawStringCenterScaled(lineY, "Version 1.x", RGB565(200, 200, 200), RGB565(20, 20, 20), 2);
        lineY += 35;
        GFX_DrawStringCenterScaled(lineY, __DATE__, RGB565(180, 180, 180), RGB565(20, 20, 20), 2);
        lineY += 35;
        GFX_DrawStringCenterScaled(lineY, "Maker: Uronour", RGB565(200, 200, 200), RGB565(20, 20, 20), 2);
        lineY += 35;
        GFX_DrawStringCenterScaled(lineY, "AI: OpenCode (claude-sonnet)", RGB565(180, 180, 180), RGB565(20, 20, 20), 2);
        
        lineY += 45;
        GFX_DrawStringCenterScaled(lineY, "github.com/uronour/", RGB565(120, 120, 120), RGB565(20, 20, 20), 1);
        lineY += 25;
        GFX_DrawStringCenterScaled(lineY, "suzuki-ecu-tool", RGB565(120, 120, 120), RGB565(20, 20, 20), 1);
        
        lineY += 35;
        GFX_DrawStringCenterScaled(lineY, "GPL-3.0", RGB565(80, 80, 80), RGB565(20, 20, 20), 1);
        g_aboutDrawn = 1;
        g_pageDirty = 0;
      }
      break;
    }
  }

  char pageStr[8];
  snprintf(pageStr, sizeof(pageStr), "%d/%d", g_currentPage + 1, GAUGE_PAGE_COUNT);
  GFX_DrawStringCenter(PAGE_FOOTER_Y, pageStr, COLOR_GRAY, RGB565(30, 30, 30));
}

void Gauge_SetPage(GaugePage page)
{
  if (page < GAUGE_PAGE_COUNT)
  {
    g_currentPage = page;
    g_prevNeedleVal = 0xFFFFFFFF;
    for (int i = 0; i < 4; i++) g_prevDig[i] = 0xFFFFFFFF;
    g_detailMode = 0;
    g_detailDrawn = 0;
    for (int i = 0; i < 8; i++) g_prevDetailVals[i] = 0xFFFFFFFF;
    memset(g_prevRpmStr, 0, sizeof(g_prevRpmStr));
    for (int i = 0; i < 4; i++) g_prevSensors[i] = 0xFFFFFFFF;
    g_prevDtcCount = 0xFFFF;
    for (int i = 0; i < 4; i++) g_prevSettings[i] = 0xFF;
    g_aboutDrawn = 0;
    g_pageDirty = 1;

    if (page == GAUGE_PAGE_DASHBOARD)
    {
      LCD_Fill(0, 0, LCD_WIDTH - 1, LCD_HEIGHT - 1, RGB565(30, 30, 30));
      DrawTachoFace();
      DigLabel(0, "Coolant");
      DigLabel(1, "Speed");
      DigLabel(2, "Battery");
      DigLabel(3, "Gear");
    }
    Gauge_Update();
  }
}

GaugePage Gauge_GetPage(void) { return g_currentPage; }

void Gauge_Press(void)
{
  if (g_currentPage == GAUGE_PAGE_DASHBOARD)
  {
    g_detailMode = !g_detailMode;
    if (g_detailMode)
    {
      LCD_FillRect(10, 160, 290, 310, RGB565(20, 20, 20));
      GFX_DrawStringScaled(15, 165, "MAP", RGB565(100, 100, 100), RGB565(20, 20, 20), 1);
      GFX_DrawStringScaled(15, 185, "IAT", RGB565(100, 100, 100), RGB565(20, 20, 20), 1);
      GFX_DrawStringScaled(15, 205, "TPS", RGB565(100, 100, 100), RGB565(20, 20, 20), 1);
      GFX_DrawStringScaled(15, 225, "O2", RGB565(100, 100, 100), RGB565(20, 20, 20), 1);
      GFX_DrawStringScaled(15, 245, "Inject", RGB565(100, 100, 100), RGB565(20, 20, 20), 1);
      GFX_DrawStringScaled(15, 265, "Ign", RGB565(100, 100, 100), RGB565(20, 20, 20), 1);
      GFX_DrawStringScaled(15, 285, "IAC", RGB565(100, 100, 100), RGB565(20, 20, 20), 1);
    }
  }
  else if (g_currentPage == GAUGE_PAGE_SETTINGS)
  {
    if (SD_Log_IsActive())
      SD_Log_Stop();
    else
      SD_Log_Start();
  }
}

void Gauge_NextPage(void)
{
  Gauge_SetPage((g_currentPage + 1) % GAUGE_PAGE_COUNT);
}

void Gauge_PrevPage(void)
{
  Gauge_SetPage((g_currentPage == 0) ? (GAUGE_PAGE_COUNT - 1) : (g_currentPage - 1));
}

void Gauge_SetDashStyle(DashStyle style)
{
  if (style < DASH_STYLE_COUNT)
  {
    g_dashStyle = style;
    g_pageDirty = 1;
    Gauge_Update();
  }
}

DashStyle Gauge_GetDashStyle(void)
{
  return g_dashStyle;
}
