#include <stdio.h>
#include <string.h>
#include "gauge_ui.h"
#include "ili9488_gfx.h"
#include "font_8x13.h"
#include "os_timer.h"

#ifdef EMULATOR_BUILD
  #include "sim_data.h"
#else
  #include "SDSProtocol.h"
  #include "kline_task.h"
  #include "bt_stream.h"
  #include "sd_log.h"
  #include "LCD_Init.h"
  #include "touch_calib.h"
#endif

#define PAGE_FOOTER_Y (g_lcdHeight - FONT_H - 4)
#define COL_LABEL  20
#define COL_VALUE  300

typedef struct {
  const char *name;
  const char *unitFmt;
  uint32_t lastVal;
} SensorField;

static GaugePage g_currentPage = GAUGE_PAGE_LIVE_DATA;
static uint32_t g_lastUpdate = 0;
static uint8_t g_pageDirty = 1;
static uint8_t g_ecoToolSel = 0;     // main menu selection (0-6)
static uint8_t g_ecoToolMode = 0;    // 0=menu, 1-7=sub-screen
static uint8_t g_ecoSubSel = 0;      // selection within sub-screen

// Shared state for memory operations
static uint32_t g_memAddr = 0x8000;
static uint32_t g_memLen = 16;
static uint8_t  g_prevDtcCount = 0xFFFF;
static uint32_t g_prevDetailVals[8] = {0xFFFFFFFF};
static uint8_t  g_detailDrawn = 0;
static uint8_t  g_aboutDrawn = 0;
static uint8_t  g_portraitMode = 0;
static uint8_t  g_stickyMode = 0;
static uint8_t  g_prevSettings[4] = {0xFF, 0xFF, 0xFF, 0xFF};

// Field editing for ECU Tools sub-screens
static uint8_t g_ecoFieldEdit = 0;   // 0=nav, 1=editing
static uint8_t g_writeData[16] = {0xAA, 0xBB, 0xCC, 0xDD};
static uint8_t g_writeLen = 4;

static const char *gearStr[] = { "N", "1", "2", "3", "4", "5", "6" };

#define SENSOR_COUNT 16
static SensorField g_sensors[SENSOR_COUNT] = {
  { "RPM",            "%lu rpm",          0 },
  { "Speed",          "%lu km/h",         0 },
  { "Coolant",        "%lu C",            0 },
  { "IAT",            "%lu C",            0 },
  { "TPS",            "%lu %%",           0 },
  { "MAP",            "%lu kPa",          0 },
  { "O2",             "%lu",              0 },
  { "Battery",        "%lu.%lu V",        0 },
  { "Gear",           "%s",               0 },
  { "STPS",           "%lu %%",           0 },
  { "Ignition",       "%lu deg",          0 },
  { "Injector",       "%lu ms",           0 },
  { "IAC Steps",      "%lu",              0 },
  { "Baro",           "%lu kPa",          0 },
  { "Neutral",        "%s",               0 },
  { "Clutch",         "%s",               0 },
};

static const char *onoff(uint32_t v) { return v ? "ON" : "OFF"; }
static const char *inout(uint32_t v) { return v ? "IN" : "OUT"; }

static uint32_t sensor_val(int i)
{
  switch (i) {
    case 0:  return g_sdsData.rpm;
    case 1:  return g_sdsData.speed;
    case 2:  return g_sdsData.coolantTemp;
    case 3:  return g_sdsData.intakeAirTemp;
    case 4:  return g_sdsData.throttlePos;
    case 5:  return g_sdsData.mapKpa;
    case 6:  return g_sdsData.o2Sensor;
    case 7:  return g_sdsData.batteryVolt;
    case 8:  return g_sdsData.gearPos;
    case 9:  return g_sdsData.stps;
    case 10: return g_sdsData.ignitionTiming;
    case 11: return g_sdsData.injectorPulse;
    case 12: return g_sdsData.iacStep;
    case 13: return g_sdsData.baroKpa;
    case 14: return g_sdsData.neutral;
    case 15: return g_sdsData.clutchIn;
    default: return 0;
  }
}

static void fmt_sensor(int i, char *buf, int bufsz)
{
  uint32_t v = sensor_val(i);
  switch (i) {
    case 7:
      snprintf(buf, bufsz, "%.1f V", (double)(v * 0.1f));
      break;
    case 8: snprintf(buf, bufsz, "%s", gearStr[v > 6 ? 0 : v]); break;
    case 14: snprintf(buf, bufsz, "%s", onoff(v)); break;
    case 15: snprintf(buf, bufsz, "%s", inout(v)); break;
    default:
      if (i == 1) v = v / 100;
      snprintf(buf, bufsz, g_sensors[i].unitFmt, (unsigned long)v);
      break;
  }
}

// ─── Sub-screen helpers ────────────────────────────────────────

#define ITEM_H 30
#define ITEM_Y(i) (40 + (i) * ITEM_H)

static int subItemCount(void)
{
  switch (g_ecoToolMode) {
    case 1: return 3; // [Execute][Back]
    case 2: return 2; // [Toggle][Back]
    case 3: return 4; // [Addr: 0x8000][Len: 16][Execute][Back]
    case 4: return 4; // [Addr: 0x8000][Data: ...][Write][Back]
    case 5: return 2; // [Dump][Back]
    case 6: return 2; // [Yes, Stop][Back]
    case 7: return 1; // [Back] (toggles immediately)
    default: return 0;
  }
}

static void drawSubHeader(const char *title, uint16_t headerCol)
{
  uint16_t bg = RGB565(18, 18, 18);
  LCD_Fill(0, 0, g_lcdWidth - 1, g_lcdHeight - 21, bg);
  GFX_DrawStringScaled(6, 4, title, headerCol, bg, 2);
  char tmp[16];
  snprintf(tmp, sizeof(tmp), "Tool %d/7", g_ecoToolMode);
  GFX_DrawString(g_lcdWidth - 100, 6, tmp, RGB565(120, 120, 120), bg);
  LCD_DrawHLine(6, g_lcdWidth - 6, 24, RGB565(40, 40, 40));
}

static void drawSubItem(int idx, const char *label, uint16_t color)
{
  uint16_t selBg = RGB565(30, 30, 40);
  uint16_t bg = RGB565(18, 18, 18);
  int16_t y = ITEM_Y(idx);
  if (idx == g_ecoSubSel) {
    LCD_FillRect(COL_LABEL - 4, y, g_lcdWidth - 20, y + ITEM_H - 2, selBg);
    GFX_DrawStringScaled(COL_LABEL - 12, y + 4, ">", color, selBg, 2);
  } else {
    LCD_FillRect(COL_LABEL - 4, y, g_lcdWidth - 20, y + ITEM_H - 2, bg);
  }
  GFX_DrawStringScaled(COL_LABEL, y + 4, label, color, bg, 2);
}

static void drawSubBack(void)
{
  int last = subItemCount() - 1;
  drawSubItem(last, "[ Back to Menu ]", RGB565(200, 200, 100));
}

static void drawSubText(int y, const char *text, uint16_t color)
{
  uint16_t bg = RGB565(18, 18, 18);
  GFX_DrawStringScaled(COL_LABEL, y, text, color, bg, 1);
}

// ─── Sub-screen Draw functions ─────────────────────────────────

static void DrawEcuInfoSub(void)
{
  drawSubHeader("READ ECU INFO", RGB565(0, 200, 80));

  #ifdef EMULATOR_BUILD
    char buf[64];
    snprintf(buf, sizeof(buf), "VIN: %.17s", g_sdsEcuInfo.vin);
    drawSubText(36, buf, RGB565(200, 200, 200));
    snprintf(buf, sizeof(buf), "Flash: %lu KB", (unsigned long)(g_sdsEcuInfo.flashSize / 1024));
    drawSubText(56, buf, RGB565(200, 200, 200));
    snprintf(buf, sizeof(buf), "Cal Offset: 0x%06lX", (unsigned long)g_sdsEcuInfo.calOffset);
    drawSubText(76, buf, RGB565(140, 140, 140));
    snprintf(buf, sizeof(buf), "Cal Size: %lu KB", (unsigned long)(g_sdsEcuInfo.calSize / 1024));
    drawSubText(96, buf, RGB565(140, 140, 140));
    drawSubText(116, "ECU ID: JS111-01 SUZUKI", RGB565(140, 200, 140));
    drawSubText(136, "SW Ver: 2A6C0012", RGB565(140, 200, 140));
  #else
    drawSubText(36, "Reading ECU info...", RGB565(200, 200, 100));
    SDS_EcuInfo info;
    if (SDS_ReadEcuInfo(&info)) {
      char buf[64];
      snprintf(buf, sizeof(buf), "VIN: %.17s", info.vin);
      drawSubText(56, buf, RGB565(200, 200, 200));
      snprintf(buf, sizeof(buf), "Flash: %lu KB", (unsigned long)(info.flashSize / 1024));
      drawSubText(76, buf, RGB565(200, 200, 200));
    } else {
      drawSubText(56, "Read failed", RGB565(200, 50, 50));
    }
  #endif

  drawSubBack();
}

static void DrawDealerModeSub(void)
{
  drawSubHeader("ENTER DEALER MODE", RGB565(255, 180, 0));

  uint8_t dealerOn = 0;
  #ifdef EMULATOR_BUILD
    dealerOn = g_dealerMode;
  #endif

  drawSubText(36, "Dealer mode grants access to", RGB565(140, 140, 140));
  drawSubText(56, "extended diagnostic functions.", RGB565(140, 140, 140));

  char buf[48];
  snprintf(buf, sizeof(buf), "Status: %s", dealerOn ? "ACTIVE" : "INACTIVE");
  drawSubText(86, buf, dealerOn ? RGB565(0, 200, 80) : RGB565(200, 100, 100));

  drawSubItem(0, dealerOn ? "[ Deactivate ]" : "[  Activate  ]", dealerOn ? RGB565(200, 100, 100) : RGB565(0, 200, 80));
  drawSubBack();
}

static void DrawReadMemSub(void)
{
  drawSubHeader("READ MEMORY BLOCK", RGB565(0, 180, 255));

  char buf[48];
  uint16_t addrCol = (g_ecoSubSel == 0 && g_ecoFieldEdit) ? RGB565(255, 255, 0) : RGB565(200, 200, 200);
  snprintf(buf, sizeof(buf), "Address: 0x%06lX%s", (unsigned long)g_memAddr,
           (g_ecoSubSel == 0 && g_ecoFieldEdit) ? " <EDIT>" : "");
  drawSubItem(0, buf, addrCol);
  uint16_t lenCol = (g_ecoSubSel == 1 && g_ecoFieldEdit) ? RGB565(255, 255, 0) : RGB565(200, 200, 200);
  snprintf(buf, sizeof(buf), "Length: %lu bytes%s", (unsigned long)g_memLen,
           (g_ecoSubSel == 1 && g_ecoFieldEdit) ? " <EDIT>" : "");
  drawSubItem(1, buf, lenCol);

  drawSubItem(2, "[ Execute Read ]", RGB565(0, 200, 80));
  drawSubBack();

  if (g_ecoFieldEdit) {
    drawSubText(150, "Encoder: adjust value, Press: confirm", RGB565(100, 200, 255));
  }
}

static void DrawWriteMemSub(void)
{
  drawSubHeader("WRITE MEMORY BLOCK", RGB565(255, 100, 100));

  char buf[56];
  uint16_t addrCol = (g_ecoSubSel == 0 && g_ecoFieldEdit) ? RGB565(255, 255, 0) : RGB565(200, 200, 200);
  snprintf(buf, sizeof(buf), "Address: 0x%06lX%s", (unsigned long)g_memAddr,
           (g_ecoSubSel == 0 && g_ecoFieldEdit) ? " <EDIT>" : "");
  drawSubItem(0, buf, addrCol);

  char dataStr[48];
  int pos = 0;
  for (int i = 0; i < g_writeLen && pos < 44; i++)
    pos += snprintf(dataStr + pos, sizeof(dataStr) - pos, "%02X ", g_writeData[i]);
  dataStr[pos > 0 ? pos - 1 : 0] = '\0';
  uint16_t dataCol = (g_ecoSubSel == 1 && g_ecoFieldEdit) ? RGB565(255, 255, 0) : RGB565(200, 200, 200);
  snprintf(buf, sizeof(buf), "Data: %s%s", dataStr,
           (g_ecoSubSel == 1 && g_ecoFieldEdit) ? " <EDIT>" : "");
  drawSubItem(1, buf, dataCol);

  drawSubItem(2, "[ Execute Write ]", RGB565(200, 50, 50));
  drawSubBack();

  if (g_ecoFieldEdit) {
    drawSubText(150, "Encoder: adjust value, Press: confirm", RGB565(100, 200, 255));
  }
}

static void DrawDumpCalSub(void)
{
  drawSubHeader("DUMP CALIBRATION", RGB565(0, 200, 200));

  drawSubText(36, "Download calibration from ECU", RGB565(140, 140, 140));
  drawSubText(56, "and save to SD card.", RGB565(140, 140, 140));

  #ifdef EMULATOR_BUILD
    uint32_t calSize = g_sdsEcuInfo.calSize;
  #else
    uint32_t calSize = g_sdsEcuInfo.calSize;
  #endif

  char buf[48];
  snprintf(buf, sizeof(buf), "Size: %lu KB", (unsigned long)(calSize / 1024));
  drawSubText(86, buf, RGB565(200, 200, 200));
  drawSubText(106, "SD: ", RGB565(140, 140, 140));

  uint8_t sdMounted = 0;
  #ifndef EMULATOR_BUILD
    sdMounted = SD_Log_IsMounted();
  #else
    sdMounted = 1; // simulate
  #endif
  snprintf(buf, sizeof(buf), "SD: %s", sdMounted ? "MOUNTED" : "NO CARD");
  drawSubText(106, buf, sdMounted ? RGB565(0, 200, 80) : RGB565(200, 50, 50));

  drawSubItem(0, "[ Start Dump ]", RGB565(0, 200, 80));
  drawSubBack();
}

static void DrawStopCommSub(void)
{
  drawSubHeader("STOP COMMUNICATION", RGB565(200, 200, 100));

  uint8_t connected = 1;
  #ifdef EMULATOR_BUILD
    connected = g_klineConnected;
  #else
    connected = KLine_IsConnected();
  #endif

  if (connected) {
    drawSubText(36, "This will stop K-Line", RGB565(140, 140, 140));
    drawSubText(56, "communication with the ECU.", RGB565(140, 140, 140));
    drawSubText(86, "Continue?", RGB565(200, 200, 100));
    drawSubItem(0, "[ Yes, Stop ]", RGB565(200, 50, 50));
  } else {
    drawSubText(36, "K-Line is not connected.", RGB565(200, 100, 100));
  }
  drawSubBack();
}

static void DrawRotateSub(void)
{
  drawSubHeader("ROTATE DISPLAY", RGB565(180, 180, 180));

  drawSubText(36, "Display orientation toggled.", RGB565(0, 200, 80));
  drawSubText(56, g_portraitMode ? "Now: Portrait (320x480)" : "Now: Landscape (480x320)", RGB565(200, 200, 200));

  drawSubBack();
}

// ─── RPM bar helper ────────────────────────────────────────────

static void DrawRpmBar(int16_t x, int16_t y, int16_t w, int16_t h, uint32_t rpm)
{
  uint16_t barBg = RGB565(15, 15, 20);
  LCD_FillRect(x, y, x + w - 1, y + h - 1, barBg);
  LCD_DrawRect(x, y, x + w - 1, y + h - 1, RGB565(40, 40, 50));

  if (rpm > 15500) rpm = 15500;
  int fillH = (int)((uint64_t)rpm * h / 15500);
  if (fillH < 0) fillH = 0;

  int fillY = y + h - fillH;
  uint16_t barCol;
  if (rpm < 8000)
    barCol = RGB565(0, 220, 80);
  else if (rpm < 12000)
    barCol = RGB565(220, 200, 0);
  else
    barCol = RGB565(220, 40, 40);

  LCD_FillRect(x + 2, fillY, x + w - 3, y + h - 2, barCol);
}

// ─── Info card helper ──────────────────────────────────────────

static void DrawInfoCard(int16_t x, int16_t y, int16_t w, int16_t h,
                          const char *label, const char *value,
                          uint16_t labelCol, uint16_t valCol, uint16_t accent)
{
  LCD_FillRect(x, y, x + w - 1, y + h - 1, RGB565(18, 18, 25));
  LCD_DrawHLine(x, x + w - 1, y, accent);
  GFX_DrawStringScaled(x + 4, y + 4, label, labelCol, RGB565(18, 18, 25), 1);
  GFX_DrawStringScaled(x + 4, y + 20, value, valCol, RGB565(18, 18, 25), 2);
}

// ─── Dashboard Draw ────────────────────────────────────────────

static void DrawDashboardLandscape(void)
{
  uint16_t bg = RGB565(10, 10, 15);
  LCD_Fill(0, 0, g_lcdWidth - 1, g_lcdHeight - 21, bg);

  // Header
  GFX_DrawStringScaled(6, 4, "DASHBOARD", RGB565(0, 180, 255), bg, 2);
  uint8_t connected = 1;
  #ifndef EMULATOR_BUILD
    connected = KLine_IsConnected();
  #endif
  GFX_DrawStringScaled(180, 6, "K:", RGB565(140, 140, 140), bg, 1);
  GFX_DrawStringScaled(200, 6, connected ? "OK" : "NC",
    connected ? RGB565(0, 200, 80) : RGB565(200, 50, 50), bg, 1);
  char tmp[8];
  snprintf(tmp, sizeof(tmp), "%d/%d", g_currentPage + 1, GAUGE_PAGE_COUNT);
  GFX_DrawString(g_lcdWidth - 50, 6, tmp, RGB565(120, 120, 120), bg);
  LCD_DrawHLine(6, g_lcdWidth - 6, 24, RGB565(30, 30, 40));

  // RPM bar on left
  DrawRpmBar(10, 30, 28, 200, g_sdsData.rpm);
  // RPM label next to bar
  GFX_DrawStringScaled(42, 34, "RPM", RGB565(100, 100, 120), bg, 1);
  char rpmStr[12];
  snprintf(rpmStr, sizeof(rpmStr), "%lu", (unsigned long)g_sdsData.rpm);
  GFX_DrawStringScaled(42, 50, rpmStr, RGB565(0, 220, 80), bg, 2);

  // Speed - large center area
  uint32_t speedKmh = g_sdsData.speed / 100;
  char spdStr[12];
  snprintf(spdStr, sizeof(spdStr), "%lu", (unsigned long)speedKmh);
  int16_t spdX = 160;
  GFX_DrawStringScaled(spdX, 60, spdStr, RGB565(0, 180, 255), bg, 5);
  GFX_DrawStringScaled(spdX, 110, "km/h", RGB565(100, 120, 140), bg, 1);

  // Gear - large
  char gearStrBuf[4];
  snprintf(gearStrBuf, sizeof(gearStrBuf), "%s", gearStr[g_sdsData.gearPos > 6 ? 0 : g_sdsData.gearPos]);
  GFX_DrawStringScaled(spdX + 90, 60, "Gear", RGB565(100, 100, 120), bg, 1);
  GFX_DrawStringScaled(spdX + 90, 80, gearStrBuf, RGB565(200, 200, 80), bg, 4);

  // Quick stats row
  int16_t qx = spdX;
  int16_t qy = 140;
  int16_t qw = (g_lcdWidth - spdX - 20) / 2;
  char qbuf[16];

  snprintf(qbuf, sizeof(qbuf), "%lu C", (unsigned long)g_sdsData.coolantTemp);
  DrawInfoCard(qx, qy, qw - 4, 42, "Coolant", qbuf, RGB565(140, 140, 200), RGB565(220, 220, 220), RGB565(60, 60, 140));

  snprintf(qbuf, sizeof(qbuf), "%.1fV", (double)(g_sdsData.batteryVolt * 0.1f));
  DrawInfoCard(qx + qw, qy, qw - 4, 42, "Battery", qbuf, RGB565(200, 200, 140), RGB565(220, 220, 220), RGB565(140, 140, 60));

  qy += 48;
  snprintf(qbuf, sizeof(qbuf), "%lu%%", (unsigned long)g_sdsData.throttlePos);
  DrawInfoCard(qx, qy, qw - 4, 42, "TPS", qbuf, RGB565(200, 140, 140), RGB565(220, 220, 220), RGB565(140, 60, 60));

  snprintf(qbuf, sizeof(qbuf), "%lu kPa", (unsigned long)g_sdsData.mapKpa);
  DrawInfoCard(qx + qw, qy, qw - 4, 42, "MAP", qbuf, RGB565(140, 200, 200), RGB565(220, 220, 220), RGB565(60, 140, 140));

  // Bottom info cards row
  int16_t cardY = 210;
  int16_t cardW = (g_lcdWidth - 50) / 4;
  int16_t cardH = 60;

  snprintf(qbuf, sizeof(qbuf), "%lu C", (unsigned long)g_sdsData.intakeAirTemp);
  DrawInfoCard(10, cardY, cardW, cardH, "IAT", qbuf, RGB565(140, 140, 200), RGB565(220, 220, 220), RGB565(80, 80, 160));

  snprintf(qbuf, sizeof(qbuf), "%lu ms", (unsigned long)g_sdsData.injectorPulse);
  DrawInfoCard(15 + cardW, cardY, cardW, cardH, "Inj", qbuf, RGB565(200, 140, 200), RGB565(220, 220, 220), RGB565(160, 60, 160));

  snprintf(qbuf, sizeof(qbuf), "%lu deg", (unsigned long)g_sdsData.ignitionTiming);
  DrawInfoCard(20 + cardW * 2, cardY, cardW, cardH, "Ign", qbuf, RGB565(200, 200, 140), RGB565(220, 220, 220), RGB565(160, 160, 60));

  snprintf(qbuf, sizeof(qbuf), "%lu%%", (unsigned long)g_sdsData.stps);
  DrawInfoCard(25 + cardW * 3, cardY, cardW, cardH, "STPS", qbuf, RGB565(140, 200, 200), RGB565(220, 220, 220), RGB565(60, 160, 160));

  for (int i = 0; i < SENSOR_COUNT; i++)
    g_sensors[i].lastVal = 0xFFFFFFFF;
}

static void DrawDashboardPortrait(void)
{
  uint16_t bg = RGB565(10, 10, 15);
  LCD_Fill(0, 0, g_lcdWidth - 1, g_lcdHeight - 21, bg);

  GFX_DrawStringScaled(6, 4, "DASHBOARD", RGB565(0, 180, 255), bg, 2);
  uint8_t connected = 1;
  #ifndef EMULATOR_BUILD
    connected = KLine_IsConnected();
  #endif
  GFX_DrawStringScaled(180, 6, "K:", RGB565(140, 140, 140), bg, 1);
  GFX_DrawStringScaled(200, 6, connected ? "OK" : "NC",
    connected ? RGB565(0, 200, 80) : RGB565(200, 50, 50), bg, 1);
  char tmp[8];
  snprintf(tmp, sizeof(tmp), "%d/%d", g_currentPage + 1, GAUGE_PAGE_COUNT);
  GFX_DrawString(g_lcdWidth - 50, 6, tmp, RGB565(120, 120, 120), bg);
  LCD_DrawHLine(6, g_lcdWidth - 6, 24, RGB565(30, 30, 40));

  // RPM bar
  DrawRpmBar(10, 30, 22, 200, g_sdsData.rpm);
  char rpmStr[12];
  snprintf(rpmStr, sizeof(rpmStr), "%lu", (unsigned long)g_sdsData.rpm);
  GFX_DrawStringScaled(36, 34, "RPM", RGB565(100, 100, 120), bg, 1);
  GFX_DrawStringScaled(36, 50, rpmStr, RGB565(0, 220, 80), bg, 2);

  // Speed + Gear on right
  uint32_t speedKmh = g_sdsData.speed / 100;
  char spdStr[12];
  snprintf(spdStr, sizeof(spdStr), "%lu", (unsigned long)speedKmh);
  GFX_DrawStringScaled(140, 40, spdStr, RGB565(0, 180, 255), bg, 5);
  GFX_DrawStringScaled(140, 95, "km/h", RGB565(100, 120, 140), bg, 1);

  char gearStrBuf[4];
  snprintf(gearStrBuf, sizeof(gearStrBuf), "%s", gearStr[g_sdsData.gearPos > 6 ? 0 : g_sdsData.gearPos]);
  GFX_DrawStringScaled(230, 40, "Gear", RGB565(100, 100, 120), bg, 1);
  GFX_DrawStringScaled(230, 60, gearStrBuf, RGB565(200, 200, 80), bg, 4);

  // Info cards 2x3 grid
  int cardW = (g_lcdWidth - 30) / 2;
  int cardH = 52;
  int cardX[2] = { 10, 15 + cardW };
  int cardY = 150;

  char qbuf[16];
  snprintf(qbuf, sizeof(qbuf), "%lu C", (unsigned long)g_sdsData.coolantTemp);
  DrawInfoCard(cardX[0], cardY, cardW, cardH, "Coolant", qbuf, RGB565(140, 140, 200), RGB565(220, 220, 220), RGB565(60, 60, 140));
  snprintf(qbuf, sizeof(qbuf), "%.1fV", (double)(g_sdsData.batteryVolt * 0.1f));
  DrawInfoCard(cardX[1], cardY, cardW, cardH, "Battery", qbuf, RGB565(200, 200, 140), RGB565(220, 220, 220), RGB565(140, 140, 60));

  cardY += cardH + 6;
  snprintf(qbuf, sizeof(qbuf), "%lu%%", (unsigned long)g_sdsData.throttlePos);
  DrawInfoCard(cardX[0], cardY, cardW, cardH, "TPS", qbuf, RGB565(200, 140, 140), RGB565(220, 220, 220), RGB565(140, 60, 60));
  snprintf(qbuf, sizeof(qbuf), "%lu kPa", (unsigned long)g_sdsData.mapKpa);
  DrawInfoCard(cardX[1], cardY, cardW, cardH, "MAP", qbuf, RGB565(140, 200, 200), RGB565(220, 220, 220), RGB565(60, 140, 140));

  cardY += cardH + 6;
  snprintf(qbuf, sizeof(qbuf), "%lu C", (unsigned long)g_sdsData.intakeAirTemp);
  DrawInfoCard(cardX[0], cardY, cardW, cardH, "IAT", qbuf, RGB565(140, 140, 200), RGB565(220, 220, 220), RGB565(80, 80, 160));
  snprintf(qbuf, sizeof(qbuf), "%lu ms", (unsigned long)g_sdsData.injectorPulse);
  DrawInfoCard(cardX[1], cardY, cardW, cardH, "Inj", qbuf, RGB565(200, 140, 200), RGB565(220, 220, 220), RGB565(160, 60, 160));

  for (int i = 0; i < SENSOR_COUNT; i++)
    g_sensors[i].lastVal = 0xFFFFFFFF;
}

// ─── Dashboard Update ──────────────────────────────────────────

static void UpdateDashboardPortrait(void)
{
  uint16_t bg = RGB565(10, 10, 15);
  static uint32_t lastRpm = 0;
  static uint32_t lastSpeed = 0;
  static uint32_t lastGear = 0;
  static uint8_t lastConn = 0xFF;
  static uint8_t firstRun = 1;

  uint8_t connected = 1;
  #ifndef EMULATOR_BUILD
    connected = KLine_IsConnected();
  #endif

  if (connected != lastConn || firstRun) {
    lastConn = connected;
    GFX_DrawStringScaled(200, 6, connected ? "OK" : "NC",
      connected ? RGB565(0, 200, 80) : RGB565(200, 50, 50), bg, 1);
  }

  char tmp[8];
  snprintf(tmp, sizeof(tmp), "%d/%d", g_currentPage + 1, GAUGE_PAGE_COUNT);
  GFX_DrawString(g_lcdWidth - 50, 6, tmp, RGB565(120, 120, 120), bg);

  if (g_sdsData.rpm != lastRpm || firstRun) {
    lastRpm = g_sdsData.rpm;
    DrawRpmBar(10, 30, 22, 200, g_sdsData.rpm);
    LCD_FillRect(36, 50, 100, 78, bg);
    char rpmStr[12];
    snprintf(rpmStr, sizeof(rpmStr), "%lu", (unsigned long)g_sdsData.rpm);
    GFX_DrawStringScaled(36, 50, rpmStr, RGB565(0, 220, 80), bg, 2);
  }

  uint32_t speedKmh = g_sdsData.speed / 100;
  if (speedKmh != lastSpeed || firstRun) {
    lastSpeed = speedKmh;
    LCD_FillRect(140, 40, 80, 50, bg);
    char spdStr[12];
    snprintf(spdStr, sizeof(spdStr), "%lu", (unsigned long)speedKmh);
    GFX_DrawStringScaled(140, 40, spdStr, RGB565(0, 180, 255), bg, 5);
  }

  if (g_sdsData.gearPos != lastGear || firstRun) {
    lastGear = g_sdsData.gearPos;
    LCD_FillRect(230, 60, 60, 50, bg);
    char gearStrBuf[4];
    snprintf(gearStrBuf, sizeof(gearStrBuf), "%s", gearStr[g_sdsData.gearPos > 6 ? 0 : g_sdsData.gearPos]);
    GFX_DrawStringScaled(230, 60, gearStrBuf, RGB565(200, 200, 80), bg, 4);
  }

  firstRun = 0;
}

static void UpdateDashboardLandscape(void)
{
  uint16_t bg = RGB565(10, 10, 15);
  static uint32_t lastRpm = 0;
  static uint32_t lastSpeed = 0;
  static uint32_t lastGear = 0;
  static uint8_t lastConn = 0xFF;
  static uint8_t firstRun = 1;

  uint8_t connected = 1;
  #ifndef EMULATOR_BUILD
    connected = KLine_IsConnected();
  #endif

  if (connected != lastConn || firstRun) {
    lastConn = connected;
    GFX_DrawStringScaled(200, 6, connected ? "OK" : "NC",
      connected ? RGB565(0, 200, 80) : RGB565(200, 50, 50), bg, 1);
  }

  char tmp[8];
  snprintf(tmp, sizeof(tmp), "%d/%d", g_currentPage + 1, GAUGE_PAGE_COUNT);
  GFX_DrawString(g_lcdWidth - 50, 6, tmp, RGB565(120, 120, 120), bg);

  if (g_sdsData.rpm != lastRpm || firstRun) {
    lastRpm = g_sdsData.rpm;
    DrawRpmBar(10, 30, 28, 200, g_sdsData.rpm);
    LCD_FillRect(42, 50, 90, 30, bg);
    char rpmStr[12];
    snprintf(rpmStr, sizeof(rpmStr), "%lu", (unsigned long)g_sdsData.rpm);
    GFX_DrawStringScaled(42, 50, rpmStr, RGB565(0, 220, 80), bg, 2);
  }

  uint32_t speedKmh = g_sdsData.speed / 100;
  if (speedKmh != lastSpeed || firstRun) {
    lastSpeed = speedKmh;
    LCD_FillRect(160, 60, 100, 50, bg);
    char spdStr[12];
    snprintf(spdStr, sizeof(spdStr), "%lu", (unsigned long)speedKmh);
    GFX_DrawStringScaled(160, 60, spdStr, RGB565(0, 180, 255), bg, 5);
  }

  if (g_sdsData.gearPos != lastGear || firstRun) {
    lastGear = g_sdsData.gearPos;
    LCD_FillRect(250, 80, 50, 50, bg);
    char gearStrBuf[4];
    snprintf(gearStrBuf, sizeof(gearStrBuf), "%s", gearStr[g_sdsData.gearPos > 6 ? 0 : g_sdsData.gearPos]);
    GFX_DrawStringScaled(250, 80, gearStrBuf, RGB565(200, 200, 80), bg, 4);
  }

  firstRun = 0;
}

static void DrawLiveData(void)
{
  if (g_portraitMode)
    DrawDashboardPortrait();
  else
    DrawDashboardLandscape();
}

static void UpdateLiveData(void)
{
  if (g_portraitMode)
    UpdateDashboardPortrait();
  else
    UpdateDashboardLandscape();
}

static void DrawDiagnostics(void)
{
  uint16_t bg = RGB565(18, 18, 18);
  uint16_t lblCol = RGB565(140, 140, 140);
  uint16_t valCol = RGB565(220, 220, 220);

  LCD_Fill(0, 0, g_lcdWidth - 1, g_lcdHeight - 21, bg);

  GFX_DrawStringScaled(6, 4, "DIAGNOSTICS", RGB565(255, 180, 0), bg, 2);
  char tmp[8];
  snprintf(tmp, sizeof(tmp), "%d/%d", g_currentPage + 1, GAUGE_PAGE_COUNT);
  GFX_DrawString(g_lcdWidth - 50, 6, tmp, RGB565(120, 120, 120), bg);
  LCD_DrawHLine(6, g_lcdWidth - 6, 24, RGB565(40, 40, 40));

  int16_t y = 36;
  GFX_DrawStringScaled(COL_LABEL, y, "DTC Count:", lblCol, bg, 2);

  g_prevDtcCount = 0;
  uint16_t dtcCount = 0;
  #ifndef EMULATOR_BUILD
    uint8_t dtcBuf[32];
    dtcCount = SDS_GetDTCs(dtcBuf, sizeof(dtcBuf));
  #endif
  g_prevDtcCount = dtcCount;

  char dtcStr[16];
  snprintf(dtcStr, sizeof(dtcStr), "%u", dtcCount);
  GFX_DrawStringScaled(260, y, dtcStr, dtcCount ? RGB565(200, 50, 50) : valCol, bg, 2);

  y += 50;
  GFX_DrawStringScaled(COL_LABEL, y, "Press ENTER to read DTCs", RGB565(100, 100, 100), bg, 1);
  y += 30;
  GFX_DrawStringScaled(COL_LABEL, y, "Hold to clear DTCs", RGB565(100, 100, 100), bg, 1);
}

static void UpdateDiagnostics(void)
{
  uint16_t bg = RGB565(18, 18, 18);

  char tmp[8];
  snprintf(tmp, sizeof(tmp), "%d/%d", g_currentPage + 1, GAUGE_PAGE_COUNT);
  GFX_DrawString(g_lcdWidth - 50, 6, tmp, RGB565(120, 120, 120), bg);

  if (g_detailDrawn)
  {
    char buf[32];
    int16_t y = 80;
    for (int i = 0; i < 6 && i < (int)g_prevDtcCount; i++)
    {
      snprintf(buf, sizeof(buf), "DTC %d: %02X", i + 1, (unsigned)(g_prevDetailVals[i] & 0xFF));
      GFX_DrawStringScaled(COL_LABEL, y + i * 25, buf, RGB565(200, 80, 80), bg, 2);
    }
  }
}

static void DrawEcuTools(void)
{
  uint16_t bg = RGB565(18, 18, 18);
  uint16_t lblCol = RGB565(140, 140, 140);
  uint16_t selCol = RGB565(0, 180, 255);

  LCD_Fill(0, 0, g_lcdWidth - 1, g_lcdHeight - 21, bg);

  GFX_DrawStringScaled(6, 4, "ECU TOOLS", RGB565(0, 200, 80), bg, 2);
  char tmp[8];
  snprintf(tmp, sizeof(tmp), "%d/%d", g_currentPage + 1, GAUGE_PAGE_COUNT);
  GFX_DrawString(g_lcdWidth - 50, 6, tmp, RGB565(120, 120, 120), bg);
  LCD_DrawHLine(6, g_lcdWidth - 6, 24, RGB565(40, 40, 40));

  const char *items[] = {
    "Read ECU Info",
    "Enter Dealer Mode",
    "Read Memory Block",
    "Write Memory Block",
    "Dump Calibration",
    "Stop Communication",
    "Rotate Display",
  };
  int itemCount = sizeof(items) / sizeof(items[0]);

  int16_t y = 40;
  for (int i = 0; i < itemCount; i++)
  {
    if (i == g_ecoToolSel) {
      GFX_DrawStringScaled(COL_LABEL, y, items[i], selCol, bg, 2);
      GFX_DrawStringScaled(COL_LABEL - 16, y, ">", selCol, bg, 2);
    } else {
      GFX_DrawStringScaled(COL_LABEL, y, items[i], lblCol, bg, 2);
    }
    y += 36;
  }

  y += 20;
  GFX_DrawStringScaled(COL_LABEL, y, "ENTER: Open tool", RGB565(100, 100, 100), bg, 1);
}

static void DrawSettings(void)
{
  uint16_t bg = RGB565(18, 18, 18);
  uint16_t lblCol = RGB565(140, 140, 140);

  LCD_Fill(0, 0, g_lcdWidth - 1, g_lcdHeight - 21, bg);

  GFX_DrawStringScaled(6, 4, "SETTINGS", RGB565(180, 180, 180), bg, 2);
  char tmp[8];
  snprintf(tmp, sizeof(tmp), "%d/%d", g_currentPage + 1, GAUGE_PAGE_COUNT);
  GFX_DrawString(g_lcdWidth - 50, 6, tmp, RGB565(120, 120, 120), bg);
  LCD_DrawHLine(6, g_lcdWidth - 6, 24, RGB565(40, 40, 40));

  uint8_t settings[4] = {0, 0, 0, 0};
  #ifndef EMULATOR_BUILD
    settings[0] = KLine_IsConnected() ? 1 : 0;
    settings[1] = BT_IsConnected() ? 1 : 0;
    settings[2] = SD_Log_IsMounted() ? 1 : 0;
    settings[3] = SD_Log_IsActive() ? 1 : 0;
  #endif
  const char *labels[4] = { "K-Line:", "BT:", "SD Card:", "Logging:" };
  const char *values[4][2] = { {"NC", "OK"}, {"NC", "OK"}, {"NC", "OK"}, {"OFF", "ON"} };
  const uint16_t colors[4][2] = {
    { RGB565(200, 50, 50), RGB565(0, 200, 80) },
    { RGB565(200, 50, 50), RGB565(0, 200, 80) },
    { RGB565(200, 50, 50), RGB565(0, 200, 80) },
    { RGB565(120, 120, 120), RGB565(0, 200, 80) }
  };

  int16_t y = 40;
  for (int i = 0; i < 4; i++)
  {
    GFX_DrawStringScaled(COL_LABEL, y, labels[i], lblCol, bg, 2);
    int16_t vw = strlen(values[i][settings[i]]) * FONT_STEP * 2;
    GFX_DrawStringScaled(COL_VALUE - vw, y, values[i][settings[i]], colors[i][settings[i]], bg, 2);
    g_prevSettings[i] = settings[i];
    y += 40;
  }

  y += 10;
  GFX_DrawStringScaled(COL_LABEL, y, "Tap: Toggle logging", RGB565(100, 100, 100), bg, 1);

  y += 30;
  GFX_DrawStringScaled(COL_LABEL, y, "Bluetooth", RGB565(0, 200, 200), bg, 2);
  GFX_DrawStringScaled(COL_LABEL, y + 22, "Tap: Reconnect BT", RGB565(100, 100, 100), bg, 1);

  y += 55;
  GFX_DrawStringScaled(COL_LABEL, y, "Calibrate Touch", RGB565(0, 180, 255), bg, 2);
  GFX_DrawStringScaled(COL_LABEL, y + 22, "Long-press here", RGB565(100, 100, 100), bg, 1);
}

static void UpdateSettings(void)
{
  uint16_t bg = RGB565(18, 18, 18);

  char tmp[8];
  snprintf(tmp, sizeof(tmp), "%d/%d", g_currentPage + 1, GAUGE_PAGE_COUNT);
  GFX_DrawString(g_lcdWidth - 50, 6, tmp, RGB565(120, 120, 120), bg);

  uint8_t settings[4] = {0, 0, 0, 0};
  #ifndef EMULATOR_BUILD
    settings[0] = KLine_IsConnected() ? 1 : 0;
    settings[1] = BT_IsConnected() ? 1 : 0;
    settings[2] = SD_Log_IsMounted() ? 1 : 0;
    settings[3] = SD_Log_IsActive() ? 1 : 0;
  #endif

  const char *values[4][2] = { {"NC", "OK"}, {"NC", "OK"}, {"NC", "OK"}, {"OFF", "ON"} };
  const uint16_t colors[4][2] = {
    { RGB565(200, 50, 50), RGB565(0, 200, 80) },
    { RGB565(200, 50, 50), RGB565(0, 200, 80) },
    { RGB565(200, 50, 50), RGB565(0, 200, 80) },
    { RGB565(120, 120, 120), RGB565(0, 200, 80) }
  };

  for (int i = 0; i < 4; i++)
  {
    if (settings[i] != g_prevSettings[i])
    {
      g_prevSettings[i] = settings[i];
      int16_t y = 40 + i * 40;
      int16_t vw = strlen(values[i][settings[i]]) * FONT_STEP * 2;
      GFX_DrawStringScaled(COL_VALUE - vw, y, values[i][settings[i]], colors[i][settings[i]], bg, 2);
    }
  }
}

static void DrawAbout(void)
{
  uint16_t bg = RGB565(12, 12, 20);
  LCD_Fill(0, 0, g_lcdWidth - 1, g_lcdHeight - 21, bg);

  int16_t y = 20;
  GFX_DrawStringCenterScaled(y, "Suzuki ECU Tool", RGB565(0, 180, 255), bg, 3);
  y += 45;
  GFX_DrawStringCenterScaled(y, "2004 GSX-R1000 K-Line", RGB565(180, 180, 180), bg, 2);
  y += 40;
  GFX_DrawStringCenterScaled(y, "Version 3.0", RGB565(200, 200, 200), bg, 2);
  y += 35;
  GFX_DrawStringCenterScaled(y, __DATE__, RGB565(180, 180, 180), bg, 2);
  y += 35;
  GFX_DrawStringCenterScaled(y, "Maker: Uronour", RGB565(200, 200, 200), bg, 2);

  y += 45;
  GFX_DrawStringCenterScaled(y, "github.com/uronour/", RGB565(120, 120, 120), bg, 1);
  y += 25;
  GFX_DrawStringCenterScaled(y, "suzuki-ecu-tool", RGB565(120, 120, 120), bg, 1);

  y += 35;
  GFX_DrawStringCenterScaled(y, "GPL-3.0", RGB565(80, 80, 80), bg, 1);

  char tmp[8];
  snprintf(tmp, sizeof(tmp), "%d/%d", g_currentPage + 1, GAUGE_PAGE_COUNT);
  GFX_DrawString(g_lcdWidth - 50, 6, tmp, RGB565(120, 120, 120), bg);

  g_aboutDrawn = 1;
}

// ─── Public API ────────────────────────────────────────────────

void Gauge_Init(void)
{
  LCD_Fill(0, 0, g_lcdWidth - 1, g_lcdHeight - 1, RGB565(18, 18, 18));
  g_currentPage = GAUGE_PAGE_LIVE_DATA;
  g_pageDirty = 1;
  g_ecoToolMode = 0;

  DrawLiveData();

  char tmp[8];
  snprintf(tmp, sizeof(tmp), "%d/%d", g_currentPage + 1, GAUGE_PAGE_COUNT);
  GFX_DrawStringCenter(PAGE_FOOTER_Y, tmp, RGB565(120, 120, 120), RGB565(18, 18, 18));
}

void Gauge_Update(void)
{
  if (OS_GetTimeMs() - g_lastUpdate < 16)
    return;
  g_lastUpdate = OS_GetTimeMs();

  switch (g_currentPage)
  {
    case GAUGE_PAGE_LIVE_DATA:
    {
      if (g_pageDirty) { DrawLiveData(); g_pageDirty = 0; }
      UpdateLiveData();
      break;
    }

    case GAUGE_PAGE_DIAGNOSTICS:
    {
      if (g_pageDirty) {
        DrawDiagnostics();
        g_pageDirty = 0;
        g_detailDrawn = 0;
      }
      UpdateDiagnostics();
      break;
    }

    case GAUGE_PAGE_ECU_TOOLS:
    {
      if (g_pageDirty) {
        if (g_ecoToolMode == 0) {
          DrawEcuTools();
        } else {
          switch (g_ecoToolMode) {
            case 1: DrawEcuInfoSub(); break;
            case 2: DrawDealerModeSub(); break;
            case 3: DrawReadMemSub(); break;
            case 4: DrawWriteMemSub(); break;
            case 5: DrawDumpCalSub(); break;
            case 6: DrawStopCommSub(); break;
            case 7: DrawRotateSub(); break;
          }
        }
        g_pageDirty = 0;
      }
      char tmp[8];
      snprintf(tmp, sizeof(tmp), "%d/%d", g_currentPage + 1, GAUGE_PAGE_COUNT);
      GFX_DrawString(g_lcdWidth - 50, 6, tmp, RGB565(120, 120, 120), RGB565(18, 18, 18));
      break;
    }

    case GAUGE_PAGE_SETTINGS:
    {
      if (g_pageDirty) { DrawSettings(); g_pageDirty = 0; }
      UpdateSettings();
      break;
    }

    case GAUGE_PAGE_ABOUT:
    {
      if (g_pageDirty || !g_aboutDrawn) { DrawAbout(); g_pageDirty = 0; }
      break;
    }
  }

  char pageStr[8];
  snprintf(pageStr, sizeof(pageStr), "%d/%d", g_currentPage + 1, GAUGE_PAGE_COUNT);
  GFX_DrawStringCenter(PAGE_FOOTER_Y, pageStr, RGB565(120, 120, 120), RGB565(18, 18, 18));
}

void Gauge_SetPage(GaugePage page)
{
  if (page < GAUGE_PAGE_COUNT)
  {
    g_currentPage = page;
    g_pageDirty = 1;
    g_ecoToolMode = 0;
    g_ecoFieldEdit = 0;
    g_detailDrawn = 0;
    for (int i = 0; i < 4; i++) g_prevSettings[i] = 0xFF;
    g_aboutDrawn = 0;
    g_prevDtcCount = 0xFFFF;

    if (page == GAUGE_PAGE_LIVE_DATA) {
      for (int i = 0; i < SENSOR_COUNT; i++)
        g_sensors[i].lastVal = 0xFFFFFFFF;
      LCD_Fill(0, 0, g_lcdWidth - 1, g_lcdHeight - 1, RGB565(18, 18, 18));
    }
    Gauge_Update();
  }
}

GaugePage Gauge_GetPage(void) { return g_currentPage; }
uint8_t Gauge_IsSticky(void) { return g_stickyMode; }

void Gauge_SetOrientation(uint8_t portrait)
{
  g_portraitMode = portrait;
  g_stickyMode = 0;
  g_pageDirty = 1;
  g_aboutDrawn = 0;
}

void Gauge_ToggleOrientation(void)
{
  g_portraitMode = !g_portraitMode;
  #ifdef EMULATOR_BUILD
    uint16_t tmp = g_lcdWidth;
    g_lcdWidth = g_lcdHeight;
    g_lcdHeight = tmp;
  #else
    LCD_RefreshDirection(g_portraitMode);
    uint16_t tmp = g_lcdWidth;
    g_lcdWidth = g_lcdHeight;
    g_lcdHeight = tmp;
  #endif
  g_stickyMode = 0;
  g_pageDirty = 1;
  g_aboutDrawn = 0;
}

void Gauge_SetSticky(uint8_t sticky)
{
  g_stickyMode = sticky;
  g_pageDirty = 1;
}

// ─── Sub-screen actions ────────────────────────────────────────

static void Ecosub_Execute(void)
{
  uint16_t bg = RGB565(18, 18, 18);

  switch (g_ecoToolMode)
  {
    case 1: // Read ECU Info (already drawn static)
      break;

    case 2: // Toggle dealer mode
    {
      #ifdef EMULATOR_BUILD
        g_dealerMode = !g_dealerMode;
        drawSubText(116, g_dealerMode ? "Dealer mode: ACTIVE" : "Dealer mode: INACTIVE",
                    g_dealerMode ? RGB565(0, 200, 80) : RGB565(200, 100, 100));
        g_pageDirty = 1;
      #else
        if (SDS_EnterDealerMode())
          drawSubText(116, "Dealer mode: ON", RGB565(0, 200, 80));
        else
          drawSubText(116, "Dealer mode failed", RGB565(200, 50, 50));
      #endif
      break;
    }

    case 3: // Read memory
    {
      #ifdef EMULATOR_BUILD
        char buf[64];
        // Simulate memory read
        drawSubText(160, "Reading from ECU...", RGB565(200, 200, 100));
        // Draw hex dump (simulated)
        int y = 180;
        if (g_memLen > 0) {
          snprintf(buf, sizeof(buf), "0x%06lX: AA BB CC DD EE FF", (unsigned long)g_memAddr);
          drawSubText(y, buf, RGB565(0, 200, 80));
          y += 18;
        }
        if (g_memLen > 8) {
          snprintf(buf, sizeof(buf), "0x%06lX: 00 11 22 33 44 55", (unsigned long)(g_memAddr + 6));
          drawSubText(y, buf, RGB565(0, 200, 80));
        }
        drawSubText(y + 20, "Dump complete (simulated)", RGB565(100, 100, 100));
      #else
        uint8_t memBuf[64];
        uint16_t got = SDS_ReadMemoryByAddress(g_memAddr, g_memLen > 64 ? 64 : g_memLen, memBuf);
        if (got > 0) {
          char hexStr[128];
          int pos = 0;
          for (int j = 0; j < got && pos < 120; j++)
            pos += snprintf(hexStr + pos, sizeof(hexStr) - pos, "%02X ", memBuf[j]);
          drawSubText(160, hexStr, RGB565(0, 200, 80));
        } else {
          drawSubText(160, "Read failed", RGB565(200, 50, 50));
        }
      #endif
      break;
    }

    case 4: // Write memory
    {
      #ifdef EMULATOR_BUILD
        char buf[48];
        snprintf(buf, sizeof(buf), "Writing %u bytes @ 0x%06lX: OK", g_writeLen, (unsigned long)g_memAddr);
        drawSubText(160, buf, RGB565(0, 200, 80));
      #else
        if (SDS_WriteMemoryByAddress(g_memAddr, g_writeLen, g_writeData))
          drawSubText(160, "Write OK", RGB565(0, 200, 80));
        else
          drawSubText(160, "Write failed", RGB565(200, 50, 50));
      #endif
      break;
    }

    case 5: // Dump calibration
    {
      #ifdef EMULATOR_BUILD
        uint32_t calSize = g_sdsEcuInfo.calSize;
        // Simulate progress
        for (int pct = 0; pct <= 100; pct += 25) {
          LCD_FillRect(20, 160, g_lcdWidth - 20, 190, bg);
          LCD_DrawProgressBar(20, 160, g_lcdWidth - 40, 25, pct, RGB565(0, 200, 80), RGB565(30, 30, 30));
          for (volatile int d = 0; d < 5000000; d++);
        }
        drawSubText(200, "Dump complete! Saved to SD.", RGB565(0, 200, 80));
        char buf[48];
        snprintf(buf, sizeof(buf), "%lu KB written", (unsigned long)(calSize / 1024));
        drawSubText(220, buf, RGB565(140, 140, 140));
      #else
        static uint8_t dumpBuf[1024];
        uint32_t dumped = SDS_DumpCalibration(dumpBuf, sizeof(dumpBuf), g_sdsEcuInfo.calOffset, g_sdsEcuInfo.calSize);
        if (dumped > 0) {
          if (SD_Log_SaveBin("0:/ecu_calibration.bin", dumpBuf, dumped)) {
            char buf[48];
            snprintf(buf, sizeof(buf), "Saved %lu bytes to SD", (unsigned long)dumped);
            drawSubText(160, buf, RGB565(0, 200, 80));
          } else {
            drawSubText(160, "SD write failed", RGB565(200, 50, 50));
          }
        } else {
          drawSubText(160, "Dump failed (no data from ECU)", RGB565(200, 50, 50));
        }
      #endif
      break;
    }

    case 6: // Stop communication
    {
      #ifdef EMULATOR_BUILD
        uint8_t wasConnected = g_klineConnected;
        g_klineConnected = 0;
        g_dealerMode = 0;
        drawSubText(120, wasConnected ? "K-Line stopped" : "Already stopped", RGB565(200, 200, 100));
        g_pageDirty = 1;
      #else
        uint8_t wasConnected = KLine_IsConnected();
        SDS_StopCommunication();
        drawSubText(120, wasConnected ? "K-Line stopped" : "Already stopped", RGB565(200, 200, 100));
        g_pageDirty = 1;
      #endif
      break;
    }

    case 7: // Rotate (already toggled)
      break;
  }
}

// ─── Press / LongPress ─────────────────────────────────────────

void Gauge_Press(void)
{
  if (g_currentPage == GAUGE_PAGE_ECU_TOOLS && g_ecoToolMode > 0)
  {
    int last = subItemCount() - 1;
    if (g_ecoSubSel == last) {
      // Back to main menu
      g_ecoToolMode = 0;
      g_ecoSubSel = 0;
      g_ecoFieldEdit = 0;
      g_pageDirty = 1;
      return;
    }

    // If in field edit mode, pressing confirms the edit and exits edit mode
    if (g_ecoFieldEdit) {
      g_ecoFieldEdit = 0;
      g_pageDirty = 1;
      return;
    }

    // Check if pressing an editable field (items 0 or 1 in Read/Write sub-screens)
    if ((g_ecoToolMode == 3 || g_ecoToolMode == 4) && (g_ecoSubSel == 0 || g_ecoSubSel == 1)) {
      g_ecoFieldEdit = 1;
      g_pageDirty = 1;
      return;
    }

    // Specific actions
    switch (g_ecoToolMode)
    {
      case 1: // ECU Info: no-op on Execute, already displayed
        break;
      case 2: // Dealer mode: toggle
        if (g_ecoSubSel == 0) {
          Ecosub_Execute();
          g_pageDirty = 1;
        }
        break;
      case 3: // Read memory: execute on item 2
        if (g_ecoSubSel == 2) Ecosub_Execute();
        break;
      case 4: // Write memory: execute on item 2
        if (g_ecoSubSel == 2) Ecosub_Execute();
        break;
      case 5: // Dump: execute on item 0
        if (g_ecoSubSel == 0) Ecosub_Execute();
        break;
      case 6: // Stop: execute on item 0
        if (g_ecoSubSel == 0) Ecosub_Execute();
        break;
      case 7: // Rotate: toggles immediately
        Gauge_ToggleOrientation();
        g_ecoToolMode = 0;
        g_ecoFieldEdit = 0;
        g_pageDirty = 1;
        return;
    }
    return;
  }

  switch (g_currentPage)
  {
    case GAUGE_PAGE_LIVE_DATA:
    case GAUGE_PAGE_ABOUT:
      break;

    case GAUGE_PAGE_DIAGNOSTICS:
    {
      uint16_t bg = RGB565(18, 18, 18);
      g_detailDrawn = 1;

      uint8_t dtcBuf[32];
      uint16_t count = 0;
      #ifndef EMULATOR_BUILD
        count = SDS_GetDTCs(dtcBuf, sizeof(dtcBuf));
      #endif
      g_prevDtcCount = count;

      char buf[32];
      snprintf(buf, sizeof(buf), "DTC Count: %u", count);
      LCD_FillRect(COL_LABEL - 4, 36, g_lcdWidth - 20, 36 + 26, bg);
      GFX_DrawStringScaled(COL_LABEL, 36, buf, count ? RGB565(200, 50, 50) : RGB565(0, 200, 80), bg, 2);

      LCD_FillRect(COL_LABEL - 4, 80, g_lcdWidth - 20, 240, bg);
      for (int i = 0; i < 6 && i < count; i++)
      {
        g_prevDetailVals[i] = dtcBuf[i];
        snprintf(buf, sizeof(buf), "DTC %d: %02X", i + 1, dtcBuf[i]);
        GFX_DrawStringScaled(COL_LABEL + 10, 80 + i * 25, buf, RGB565(200, 80, 80), bg, 2);
      }
      if (count == 0)
        GFX_DrawStringScaled(COL_LABEL + 10, 80, "No DTCs found", RGB565(100, 200, 100), bg, 2);
      break;
    }

    case GAUGE_PAGE_ECU_TOOLS:
    {
      // Main menu: enter sub-screen
      g_ecoToolMode = g_ecoToolSel + 1;
      g_ecoSubSel = 0;

      // Tool 7 (Rotate) toggles immediately
      if (g_ecoToolMode == 7) {
        Gauge_ToggleOrientation();
        g_ecoToolMode = 0;
      }

      g_pageDirty = 1;
      break;
    }

    case GAUGE_PAGE_SETTINGS:
    {
      #ifndef EMULATOR_BUILD
        if (SD_Log_IsActive()) SD_Log_Stop(); else SD_Log_Start();
      #endif
      break;
    }
  }
}

void Gauge_LongPress(void)
{
  if (g_currentPage == GAUGE_PAGE_DIAGNOSTICS)
  {
    #ifndef EMULATOR_BUILD
      SDS_ClearDTCs();
    #endif
    LCD_FillRect(COL_LABEL - 4, 70, g_lcdWidth - 20, 240, RGB565(18, 18, 18));
    GFX_DrawStringScaled(COL_LABEL + 10, 80, "DTCs cleared", RGB565(0, 200, 80), RGB565(18, 18, 18), 2);
    g_prevDtcCount = 0;
  }
  else if (g_currentPage == GAUGE_PAGE_SETTINGS)
  {
    #ifndef EMULATOR_BUILD
      TP_Calibrate();
      g_pageDirty = 1;
    #endif
  }
}

// ─── Select Up/Down ────────────────────────────────────────────

void Gauge_SelectUp(void)
{
  if (g_currentPage == GAUGE_PAGE_ECU_TOOLS)
  {
    if (g_ecoToolMode == 0) {
      if (g_ecoToolSel > 0) { g_ecoToolSel--; g_pageDirty = 1; }
    } else if (g_ecoFieldEdit) {
      // Field edit mode: adjust value
      switch (g_ecoToolMode) {
        case 3: // Read memory
          if (g_ecoSubSel == 0) {
            if (g_memAddr >= 0x10) g_memAddr -= 0x10; else g_memAddr = 0;
          } else if (g_ecoSubSel == 1) {
            if (g_memLen > 1) g_memLen--;
          }
          break;
        case 4: // Write memory
          if (g_ecoSubSel == 0) {
            if (g_memAddr >= 0x10) g_memAddr -= 0x10; else g_memAddr = 0;
          } else if (g_ecoSubSel == 1) {
            // Adjust last data byte
            if (g_writeLen > 0) {
              if (g_writeData[g_writeLen - 1] > 0) g_writeData[g_writeLen - 1]--;
              else g_writeData[g_writeLen - 1] = 0xFF;
            }
          }
          break;
      }
      g_pageDirty = 1;
    } else {
      int cnt = subItemCount();
      if (g_ecoSubSel > 0) { g_ecoSubSel--; g_pageDirty = 1; }
    }
  }
}

void Gauge_SelectDown(void)
{
  if (g_currentPage == GAUGE_PAGE_ECU_TOOLS)
  {
    if (g_ecoToolMode == 0) {
      if (g_ecoToolSel < 6) { g_ecoToolSel++; g_pageDirty = 1; }
    } else if (g_ecoFieldEdit) {
      // Field edit mode: adjust value
      switch (g_ecoToolMode) {
        case 3: // Read memory
          if (g_ecoSubSel == 0) {
            g_memAddr += 0x10;
            if (g_memAddr > 0xFFFFFF) g_memAddr = 0xFFFFFF;
          } else if (g_ecoSubSel == 1) {
            if (g_memLen < 256) g_memLen++;
          }
          break;
        case 4: // Write memory
          if (g_ecoSubSel == 0) {
            g_memAddr += 0x10;
            if (g_memAddr > 0xFFFFFF) g_memAddr = 0xFFFFFF;
          } else if (g_ecoSubSel == 1) {
            // Adjust last data byte
            if (g_writeLen > 0) {
              if (g_writeData[g_writeLen - 1] < 0xFF) g_writeData[g_writeLen - 1]++;
              else g_writeData[g_writeLen - 1] = 0;
            }
          }
          break;
      }
      g_pageDirty = 1;
    } else {
      int cnt = subItemCount();
      if (g_ecoSubSel < cnt - 1) { g_ecoSubSel++; g_pageDirty = 1; }
    }
  }
}

// ─── Touch ─────────────────────────────────────────────────────

void Gauge_TouchAt(uint16_t x, uint16_t y)
{
  printf("[TOUCH] Gauge_TouchAt(%u,%u) page=%d mode=%d\n", x, y, g_currentPage, g_ecoToolMode);

  // Clamp coordinates to screen bounds
  if (x >= g_lcdWidth) x = g_lcdWidth - 1;
  if (y >= g_lcdHeight) y = g_lcdHeight - 1;

  if (g_currentPage == GAUGE_PAGE_ECU_TOOLS && g_ecoToolMode > 0)
  {
    int cnt = subItemCount();
    for (int i = 0; i < cnt; i++)
    {
      uint16_t y1 = ITEM_Y(i);
      uint16_t y2 = y1 + ITEM_H;
      if (y >= y1 && y < y2)
      {
        printf("[TOUCH] Sub-item %d selected (y=%u, zone %u-%u)\n", i, y, y1, y2);
        g_ecoSubSel = i;
        g_pageDirty = 1;
        Gauge_Press();
        return;
      }
    }
    printf("[TOUCH] No sub-item at y=%u (cnt=%d)\n", y, subItemCount());
    return;
  }

  switch (g_currentPage)
  {
    case GAUGE_PAGE_ECU_TOOLS:
    {
      for (int i = 0; i < 7; i++)
      {
        uint16_t y1 = 40 + i * 36;
        uint16_t y2 = y1 + 36;
        if (y >= y1 && y < y2)
        {
          printf("[TOUCH] ECU Tools item %d selected (y=%u, zone %u-%u)\n", i, y, y1, y2);
          g_ecoToolSel = i;
          g_pageDirty = 1;
          Gauge_Press();
          return;
        }
      }
      printf("[TOUCH] No ECU Tools item at y=%u\n", y);
      break;
    }

    case GAUGE_PAGE_DIAGNOSTICS:
    {
      if (y >= 80 && y < 115) { printf("[TOUCH] Read DTCs\n"); Gauge_Press(); return; }
      if (y >= 115 && y < 150) { printf("[TOUCH] Clear DTCs\n"); Gauge_LongPress(); return; }
      printf("[TOUCH] No Diagnostics zone at y=%u\n", y);
      break;
    }

    case GAUGE_PAGE_SETTINGS:
    {
      if (y >= 200 && y < 235) { printf("[TOUCH] Toggle logging\n"); Gauge_Press(); return; }
      if (y >= 240 && y < 280) {
        printf("[TOUCH] Bluetooth reconnect\n");
        #ifdef EMULATOR_BUILD
          // Simulate BT reconnect toggle
          g_btConnected = !g_btConnected;
          g_pageDirty = 1;
        #else
          BT_Init();
          g_pageDirty = 1;
        #endif
        return;
      }
      if (y >= 290 && y < 330) {
        printf("[TOUCH] Calibrate touch\n");
        #ifndef EMULATOR_BUILD
          TP_Calibrate();
        #endif
        g_pageDirty = 1; return;
      }
      printf("[TOUCH] No Settings zone at y=%u\n", y);
      break;
    }

    default:
      printf("[TOUCH] No touch handler for page %d\n", g_currentPage);
      break;
  }
}

// ─── Page navigation ───────────────────────────────────────────

void Gauge_NextPage(void)
{
  Gauge_SetPage((g_currentPage + 1) % GAUGE_PAGE_COUNT);
}

void Gauge_PrevPage(void)
{
  Gauge_SetPage((g_currentPage == 0) ? (GAUGE_PAGE_COUNT - 1) : (g_currentPage - 1));
}
