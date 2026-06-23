#include <stdio.h>
#include <string.h>
#include "gauge_ui.h"
#include "gauge_dial.h"
#include "ili9488_gfx.h"
#include "SDSProtocol.h"
#include "kline_task.h"
#include "bt_stream.h"
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

static GaugePage g_currentPage = GAUGE_PAGE_DASHBOARD;
static uint32_t g_lastUpdate = 0;
static uint32_t g_prevNeedleVal = 0xFFFFFFFF;
static uint32_t g_prevDig[4] = {0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF};

static GaugeDial g_tach = {
  .cx = TACH_CX, .cy = TACH_CY,
  .radius = TACH_R,
  .startAngle = TACH_START, .endAngle = TACH_END,
  .minVal = 0, .maxVal = TACH_MAX,
  .arcWidth = TACH_ARC_W
};

static void DigLabel(int idx, const char *label)
{
  int16_t y = DIG_LABEL_Y + idx * DIG_STEP;
  GFX_DrawString(DIG_X, y, label, COLOR_WHITE, COLOR_BLACK);
}

static void DigValue(int idx, const char *text, uint16_t color)
{
  int16_t y = DIG_VAL_Y + idx * DIG_STEP;
  LCD_FillRect(DIG_X - 4, y - 4, LCD_WIDTH - 4, y + FONT_H * DIG_SCALE, COLOR_BLACK);
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
}

void Gauge_Init(void)
{
  DIAL_InitTable();
  LCD_Fill(0, 0, LCD_WIDTH - 1, LCD_HEIGHT - 1, RGB565(30, 30, 30));
  g_currentPage = GAUGE_PAGE_DASHBOARD;
  g_prevNeedleVal = 0xFFFFFFFF;
  for (int i = 0; i < 4; i++) g_prevDig[i] = 0xFFFFFFFF;

  DrawTachoFace();
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
  if (OS_GetTimeMs() - g_lastUpdate < 80)
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

      char rpmStr[16];
      snprintf(rpmStr, sizeof(rpmStr), "%u RPM", g_sdsData.rpm);
      GFX_DrawStringScaled(TACH_CX - 50, TACH_CY + 35, rpmStr, COLOR_WHITE, RGB565(30, 30, 30), 2);

      uint32_t speedMph = (uint32_t)((unsigned long)g_sdsData.speed * 621371UL / 1000000UL);
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
      break;
    }

    case GAUGE_PAGE_SENSORS:
    {
      LCD_Fill(0, 0, LCD_WIDTH - 1, LCD_HEIGHT - 21, RGB565(30, 30, 30));
      GFX_DrawStringScaled(30, 80, "MAP", COLOR_GRAY, RGB565(30, 30, 30), 2);
      DigValueU32(0, g_sdsData.mapKpa, "kPa", COLOR_WHITE);
      GFX_DrawStringScaled(30, 80 + DIG_STEP, "O2", COLOR_GRAY, RGB565(30, 30, 30), 2);
      DigValueU32(1, g_sdsData.o2Sensor, "", COLOR_WHITE);
      GFX_DrawStringScaled(30, 80 + DIG_STEP * 2, "Ign Timing", COLOR_GRAY, RGB565(30, 30, 30), 2);
      DigValueU32(2, g_sdsData.ignitionTiming, "deg", COLOR_WHITE);
      GFX_DrawStringScaled(30, 80 + DIG_STEP * 3, "IAC Steps", COLOR_GRAY, RGB565(30, 30, 30), 2);
      DigValueU32(3, g_sdsData.iacStep, "", COLOR_WHITE);
      break;
    }

    case GAUGE_PAGE_DTC:
    {
      LCD_Fill(0, 0, LCD_WIDTH - 1, LCD_HEIGHT - 21, RGB565(30, 30, 30));
      uint8_t dtcBuf[32];
      uint16_t count = SDS_GetDTCs(dtcBuf, sizeof(dtcBuf));
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
      break;
    }

    case GAUGE_PAGE_SETTINGS:
    {
      LCD_Fill(0, 0, LCD_WIDTH - 1, LCD_HEIGHT - 21, RGB565(30, 30, 30));
      GFX_DrawStringScaled(30, 80, "K-Line:", COLOR_GRAY, RGB565(30, 30, 30), 2);
      DigValue(0, KLine_IsConnected() ? "OK" : "NC", KLine_IsConnected() ? COLOR_GREEN : COLOR_RED);
      GFX_DrawStringScaled(30, 80 + DIG_STEP, "BT:", COLOR_GRAY, RGB565(30, 30, 30), 2);
      DigValue(1, BT_IsConnected() ? "OK" : "NC", BT_IsConnected() ? COLOR_GREEN : COLOR_RED);
      GFX_DrawStringScaled(30, 80 + DIG_STEP * 2, "Dealer", COLOR_GRAY, RGB565(30, 30, 30), 2);
      GFX_DrawStringScaled(30, 80 + DIG_STEP * 3, "Log", COLOR_GRAY, RGB565(30, 30, 30), 2);
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
    LCD_Fill(0, 0, LCD_WIDTH - 1, LCD_HEIGHT - 1, RGB565(30, 30, 30));
    if (page == GAUGE_PAGE_DASHBOARD) DrawTachoFace();
    Gauge_Update();
  }
}

GaugePage Gauge_GetPage(void) { return g_currentPage; }

void Gauge_NextPage(void)
{
  g_currentPage = (g_currentPage + 1) % GAUGE_PAGE_COUNT;
  g_prevNeedleVal = 0xFFFFFFFF;
  for (int i = 0; i < 4; i++) g_prevDig[i] = 0xFFFFFFFF;
  LCD_Fill(0, 0, LCD_WIDTH - 1, LCD_HEIGHT - 1, RGB565(30, 30, 30));
  if (g_currentPage == GAUGE_PAGE_DASHBOARD) DrawTachoFace();
}

void Gauge_PrevPage(void)
{
  g_currentPage = (g_currentPage == 0) ? (GAUGE_PAGE_COUNT - 1) : (g_currentPage - 1);
  g_prevNeedleVal = 0xFFFFFFFF;
  for (int i = 0; i < 4; i++) g_prevDig[i] = 0xFFFFFFFF;
  LCD_Fill(0, 0, LCD_WIDTH - 1, LCD_HEIGHT - 1, RGB565(30, 30, 30));
  if (g_currentPage == GAUGE_PAGE_DASHBOARD) DrawTachoFace();
}
