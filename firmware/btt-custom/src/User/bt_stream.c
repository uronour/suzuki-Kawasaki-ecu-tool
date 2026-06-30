#include "bt_stream.h"
#include "includes.h"
#include "BT_ResetScript.h"
#include <stdarg.h>

static char g_txBuf[BT_TX_BUF];
static char g_rxBuf[BT_RX_BUF];
static uint8_t g_rxIdx = 0;
static bool g_ready = false;

void BT_Init(void)
{
  Serial_Config(BT_UART_PORT, BT_RX_BUF, BT_TX_BUF, BT_BAUD);
  g_rxIdx = 0;
  g_ready = true;

  BT_SendLine("Suzuki SDS ECU Tool (HC-05)");
  BT_SendLine("BT Port: UART4 (PC10/PC11)");
  BT_SendLine("K-Line: RS232 (PA2/PA3)");
}

static void BT_SendRaw(const char *str)
{
  Serial_Put(BT_UART_PORT, str);
}

void BT_SendLine(const char *fmt, ...)
{
  va_list args;
  va_start(args, fmt);
  vsnprintf(g_txBuf, BT_TX_BUF, fmt, args);
  va_end(args);
  strncat(g_txBuf, "\r\n", BT_TX_BUF - strlen(g_txBuf) - 1);
  BT_SendRaw(g_txBuf);
}

void BT_SendJSON(void)
{
  snprintf(g_txBuf, BT_TX_BUF,
    "status rpm=%u,spd=%u,cool=%u,iat=%u,tps=%u,bat=%u,"
    "gear=%u,map=%u,o2=%u,stps=%u,inj=%u,ign=%u,iac=%u,"
    "baro=%u,neut=%u,clutch=%u,fan=%u,std=%u",
    g_sdsData.rpm, g_sdsData.speed,
    g_sdsData.coolantTemp, g_sdsData.intakeAirTemp,
    g_sdsData.throttlePos, g_sdsData.batteryVolt,
    g_sdsData.gearPos, g_sdsData.mapKpa,
    g_sdsData.o2Sensor, g_sdsData.stps,
    g_sdsData.injectorPulse, g_sdsData.ignitionTiming,
    g_sdsData.iacStep, g_sdsData.baroKpa,
    g_sdsData.neutral ? 1U : 0U,
    g_sdsData.clutchIn ? 1U : 0U,
    g_sdsData.fanOn ? 1U : 0U,
    g_sdsData.sidestandDown ? 1U : 0U);
  BT_SendRaw(g_txBuf);
  BT_SendRaw("\r\n");
}

bool BT_IsConnected(void)
{
  return g_ready;
}

void BT_SendInfo(void)
{
  snprintf(g_txBuf, BT_TX_BUF,
    "info vin=%s,flash=%lu,cal_off=%lu,cal_sz=%lu",
    g_sdsEcuInfo.vin,
    (unsigned long)g_sdsEcuInfo.flashSize,
    (unsigned long)g_sdsEcuInfo.calOffset,
    (unsigned long)g_sdsEcuInfo.calSize);
  BT_SendRaw(g_txBuf);
  BT_SendRaw("\r\n");
}

static void BT_ProcessCommand(const char *cmd)
{
  printf("[ST-LINK] Processing BT Command: %s\n", cmd);
  // Trim leading spaces
  while (*cmd == ' ') cmd++;

  if (strcasecmp(cmd, "status") == 0) {
    BT_SendJSON();
  }
  else if (strcasecmp(cmd, "info") == 0) {
    BT_SendInfo();
  }
  else if (strcasecmp(cmd, "ping") == 0) {
    BT_SendLine("pong");
  }
  else if (strcasecmp(cmd, "kline_test_loopback") == 0) {
    BT_SendLine("ok Running loopback test on RS232...");
    uint8_t result = KLine_LoopbackTest();
    BT_SendLine("kline_loopback %u/8 passed", result);
  }
  else if (strncasecmp(cmd, "loopback ", 9) == 0) {
    BT_SendLine("%s", cmd);
  }
  else if (strcasecmp(cmd, "dtc") == 0) {
    uint8_t dtcBuf[32];
    uint16_t count = SDS_GetDTCs(dtcBuf, sizeof(dtcBuf));
    if (count == 0) {
      BT_SendLine("dtc none");
    } else {
      g_txBuf[0] = '\0';
      strcat(g_txBuf, "dtc ");
      for (uint16_t i = 0; i < count; i++) {
        char num[8];
        snprintf(num, sizeof(num), "%s%u", (i > 0) ? "," : "", dtcBuf[i]);
        strncat(g_txBuf, num, BT_TX_BUF - strlen(g_txBuf) - 1);
      }
      BT_SendRaw(g_txBuf);
      BT_SendRaw("\r\n");
    }
  }
  else if (strcmp(cmd, "dealer_on") == 0) {
    KLine_SetDealerMode(true);
    BT_SendLine("ok Dealer mode ON");
  }
  else if (strcmp(cmd, "dealer_off") == 0) {
    KLine_SetDealerMode(false);
    BT_SendLine("ok Dealer mode OFF");
  }
  else if (strcmp(cmd, "stream_on") == 0) {
    BT_SendLine("ok Streaming enabled");
  }
  else if (strcmp(cmd, "stream_off") == 0) {
    BT_SendLine("ok Streaming disabled");
  }
  else if (strncasecmp(cmd, "set_brand ", 10) == 0) {
    if (strstr(cmd, "kawasaki")) {
      SDS_SetBrand(BRAND_KAWASAKI);
      BT_SendLine("ok Brand set to Kawasaki");
    } else {
      SDS_SetBrand(BRAND_SUZUKI);
      BT_SendLine("ok Brand set to Suzuki");
    }
  }
  else if (strcmp(cmd, "stop") == 0) {
    BT_SendLine("stopped");
  }
  else if (strcmp(cmd, "help") == 0) {
    BT_SendLine("Commands: status, dtc, info, dealer_on, dealer_off, stream_on, stream_off, ping, stop, help, kline_test_loopback");
  }
  else if (strlen(cmd) > 0) {
    BT_SendLine("error Unknown command: %s", cmd);
  }
}

void BT_Stream(void)
{
  if (!g_ready) return;

  while (Serial_GetWritingIndexRX(BT_UART_PORT) != Serial_GetReadingIndexRX(BT_UART_PORT))
  {
    char c = dmaL1DataRX[BT_UART_PORT].cache[Serial_GetReadingIndexRX(BT_UART_PORT)];
    dmaL1DataRX[BT_UART_PORT].rIndex = (dmaL1DataRX[BT_UART_PORT].rIndex + 1) % dmaL1DataRX[BT_UART_PORT].cacheSize;

    // Debug byte through ST-Link
    printf("%c", c);

    if (c == '\r' || c == '\n')
    {
      if (g_rxIdx > 0)
      {
        g_rxBuf[g_rxIdx] = '\0';
        BT_ProcessCommand(g_rxBuf);
        g_rxIdx = 0;
      }
    }
    else if (g_rxIdx < BT_RX_BUF - 1)
    {
      g_rxBuf[g_rxIdx++] = c;
    }
  }
}
