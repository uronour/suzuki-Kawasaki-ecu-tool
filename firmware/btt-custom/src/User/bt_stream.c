#include "bt_stream.h"
#include "includes.h"
#include <stdarg.h>

static char g_btTxBuf[BT_TX_BUF];
static char g_btRxBuf[BT_RX_BUF];
static uint8_t g_btRxIdx = 0;
static uint32_t g_btLastTx = 0;
static bool g_btConnected = false;

void BT_Init(void)
{
  Serial_Config(BT_UART_PORT, BT_RX_BUF, BT_TX_BUF, BT_BAUD);
  g_btRxIdx = 0;
  g_btConnected = true;
}

static void BT_SendRaw(const char *str)
{
  Serial_Put(BT_UART_PORT, str);
}

void BT_SendLine(const char *fmt, ...)
{
  va_list args;
  va_start(args, fmt);
  vsnprintf(g_btTxBuf, BT_TX_BUF, fmt, args);
  va_end(args);
  strncat(g_btTxBuf, "\r\n", BT_TX_BUF - strlen(g_btTxBuf) - 1);
  BT_SendRaw(g_btTxBuf);
}

void BT_SendJSON(void)
{
  snprintf(g_btTxBuf, BT_TX_BUF,
    "{\"rpm\":%u,\"coolant\":%u,\"air\":%u,\"tps\":%u,\"batt\":%u,"
    "\"gear\":%u,\"inj\":%u,\"speed\":%u,\"o2\":%u,\"ign\":%u,"
    "\"map\":%u,\"fan\":%u,\"side\":%u,\"clutch\":%u}",
    g_sdsData.rpm, g_sdsData.coolantTemp, g_sdsData.intakeAirTemp,
    g_sdsData.throttlePos, g_sdsData.batteryVolt, g_sdsData.gearPos,
    g_sdsData.injectorPulse, g_sdsData.speed, g_sdsData.o2Sensor,
    g_sdsData.ignitionTiming, g_sdsData.mapKpa,
    g_sdsData.fanOn, g_sdsData.sidestandDown, g_sdsData.clutchIn);
  BT_SendRaw(g_btTxBuf);
  BT_SendRaw("\r\n");
}

bool BT_IsConnected(void)
{
  return g_btConnected;
}

void BT_ProcessCommand(const char *cmd)
{
  if (strcmp(cmd, "status") == 0)
  {
    BT_SendJSON();
  }
  else if (strcmp(cmd, "dtc") == 0)
  {
    uint8_t dtcBuf[32];
    uint16_t count = SDS_GetDTCs(dtcBuf, sizeof(dtcBuf));
    BT_SendLine("DTC count: %u", count);
  }
  else if (strcmp(cmd, "dealer_on") == 0)
  {
    KLine_SetDealerMode(true);
    BT_SendLine("Dealer mode ON");
  }
  else if (strcmp(cmd, "dealer_off") == 0)
  {
    KLine_SetDealerMode(false);
    BT_SendLine("Dealer mode OFF");
  }
  else if (strcmp(cmd, "ping") == 0)
  {
    BT_SendLine("pong");
  }
  else if (strcmp(cmd, "help") == 0)
  {
    BT_SendLine("Commands: status, dtc, dealer_on, dealer_off, ping, help, stream");
  }
  else if (strcmp(cmd, "stream") == 0)
  {
    BT_SendJSON();
  }
}

void BT_Stream(void)
{
  while (Serial_GetWritingIndexRX(BT_UART_PORT) != Serial_GetReadingIndexRX(BT_UART_PORT))
  {
    char c = dmaL1DataRX[BT_UART_PORT].cache[Serial_GetReadingIndexRX(BT_UART_PORT)];
    dmaL1DataRX[BT_UART_PORT].rIndex = (dmaL1DataRX[BT_UART_PORT].rIndex + 1) % dmaL1DataRX[BT_UART_PORT].cacheSize;

    if (c == '\r' || c == '\n')
    {
      if (g_btRxIdx > 0)
      {
        g_btRxBuf[g_btRxIdx] = '\0';
        BT_ProcessCommand(g_btRxBuf);
        g_btRxIdx = 0;
      }
    }
    else if (g_btRxIdx < BT_RX_BUF - 1)
    {
      g_btRxBuf[g_btRxIdx++] = c;
    }
  }

  if (OS_GetTimeMs() - g_btLastTx > 500)
  {
    BT_SendJSON();
    g_btLastTx = OS_GetTimeMs();
  }
}
