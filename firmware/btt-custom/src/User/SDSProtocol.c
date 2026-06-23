#include "SDSProtocol.h"
#include "includes.h"

SDS_Data g_sdsData;
volatile SDS_State g_sdsState = SDS_IDLE;
uint32_t g_sdsLastRxMs = 0;

static uint8_t g_sdsKLinePort = _USART3;

void SDS_SetKLineUART(uint8_t port)
{
  g_sdsKLinePort = port;
}

static uint8_t SDS_CalcChecksum(const uint8_t *buf, uint16_t len)
{
  uint8_t sum = 0;
  for (uint16_t i = 0; i < len; i++)
    sum += buf[i];
  return sum;
}

static bool SDS_VerifyChecksum(const uint8_t *buf, uint16_t len)
{
  if (len < 2) return false;
  uint8_t expected = SDS_CalcChecksum(buf, len - 1);
  return expected == buf[len - 1];
}

static void SDS_SendFrame(const uint8_t *pid, uint16_t pidLen)
{
  uint8_t frame[32];
  uint8_t idx = 0;
  frame[idx++] = 0x80 | pidLen;
  frame[idx++] = SDS_ECU_ADDR;
  frame[idx++] = SDS_TESTER_ADDR;
  for (uint16_t i = 0; i < pidLen; i++)
    frame[idx++] = pid[i];
  frame[idx] = SDS_CalcChecksum(frame, idx);
  KLine_SendBuf(frame, idx + 1);
}

static uint16_t SDS_ReadFrame(uint8_t *buf, uint16_t maxLen, uint32_t timeoutMs)
{
  return KLine_ReadBuf(buf, maxLen, timeoutMs);
}

static bool SDS_WaitResponse(uint8_t *buf, uint16_t maxLen, uint32_t timeoutMs)
{
  uint16_t rlen = SDS_ReadFrame(buf, maxLen, timeoutMs);
  if (rlen < 2)
    return false;
  return SDS_VerifyChecksum(buf, rlen);
}

bool SDS_Init(void)
{
  g_sdsState = SDS_INIT_WAIT_HIGH;
  memset(&g_sdsData, 0, sizeof(g_sdsData));

  KLine_FastInit();
  Delay_ms(50);

  uint8_t pidStart[] = { 0x81 };
  SDS_SendFrame(pidStart, sizeof(pidStart));

  uint8_t resp[SDS_RX_BUF_SIZE];
  if (!SDS_WaitResponse(resp, SDS_RX_BUF_SIZE, SDS_TIMEOUT_MS))
  {
    g_sdsState = SDS_ERROR;
    KLine_SetConnected(false);
    return false;
  }

  if (resp[0] != 0x80 || resp[1] != SDS_TESTER_ADDR || resp[2] != SDS_ECU_ADDR || resp[4] != 0xC1)
  {
    g_sdsState = SDS_ERROR;
    KLine_SetConnected(false);
    return false;
  }

  uint8_t pidATP[] = { 0x83, 0x02 };
  SDS_SendFrame(pidATP, sizeof(pidATP));

  if (!SDS_WaitResponse(resp, SDS_RX_BUF_SIZE, SDS_TIMEOUT_MS))
  {
    g_sdsState = SDS_ERROR;
    KLine_SetConnected(false);
    return false;
  }

  g_sdsState = SDS_ACTIVE;
  g_sdsLastRxMs = OS_GetTimeMs();
  KLine_SetConnected(true);
  return true;
}

void SDS_PollSensorData(void)
{
  if (g_sdsState != SDS_ACTIVE)
    return;

  uint8_t pid[] = { 0x21, 0x08 };
  SDS_SendFrame(pid, sizeof(pid));

  uint8_t resp[SDS_RX_BUF_SIZE];
  if (!SDS_WaitResponse(resp, SDS_RX_BUF_SIZE, SDS_TIMEOUT_MS))
    return;

  g_sdsLastRxMs = OS_GetTimeMs();
  SDS_Response sresp;
  sresp.len = resp[3] + 4;
  memcpy(sresp.data, resp, sresp.len);
  SDS_ParseSensorData(&sresp);
}

bool SDS_SendRequest(uint8_t requestId, uint8_t *payload, uint8_t len)
{
  uint8_t buf[20];
  buf[0] = requestId;
  for (uint8_t i = 0; i < len; i++)
    buf[1 + i] = payload[i];
  SDS_SendFrame(buf, 1 + len);
  return true;
}

bool SDS_ReadResponse(SDS_Response *resp)
{
  uint8_t buf[SDS_RX_BUF_SIZE];
  if (!SDS_WaitResponse(buf, SDS_RX_BUF_SIZE, SDS_TIMEOUT_MS))
    return false;

  resp->len = buf[3] + 4;
  resp->cmd = buf[4];
  memcpy(resp->data, buf, resp->len);
  return true;
}

void SDS_SendKeepAlive(void)
{
  if (g_sdsState != SDS_ACTIVE)
    return;

  uint8_t pid[] = { 0x3E, 0x01 };
  SDS_SendFrame(pid, sizeof(pid));
}

bool SDS_EnterDealerMode(void)
{
  KLine_SetDealerMode(true);
  Delay_ms(500);

  uint8_t pid[] = { 0x25, 0x01 };
  SDS_SendFrame(pid, sizeof(pid));

  uint8_t resp[SDS_RX_BUF_SIZE];
  if (!SDS_WaitResponse(resp, SDS_RX_BUF_SIZE, SDS_TIMEOUT_MS))
    return false;

  return resp[0] == 0x80 && resp[4] == 0x65;
}

uint16_t SDS_GetDTCs(uint8_t *dtcBuf, uint8_t maxLen)
{
  uint8_t pid[] = { 0x13 };
  SDS_SendFrame(pid, sizeof(pid));

  uint8_t resp[SDS_RX_BUF_SIZE];
  if (!SDS_WaitResponse(resp, SDS_RX_BUF_SIZE, SDS_TIMEOUT_MS))
    return 0;

  uint16_t count = 0;
  uint8_t len = resp[3];
  for (uint8_t i = 0; i < len && count < maxLen; i++)
    dtcBuf[count++] = resp[4 + i];
  return count;
}

bool SDS_ClearDTCs(void)
{
  uint8_t pid[] = { 0x14 };
  SDS_SendFrame(pid, sizeof(pid));

  uint8_t resp[SDS_RX_BUF_SIZE];
  if (!SDS_WaitResponse(resp, SDS_RX_BUF_SIZE, SDS_TIMEOUT_MS))
    return false;

  return resp[0] == 0x80 && resp[4] == 0x54;
}

bool SDS_ParseSensorData(const SDS_Response *resp)
{
  if (resp->len < 8)
    return false;

  if (resp->data[4] != 0x61)
    return false;

  uint8_t *d = resp->data;
  g_sdsData.rpm = d[17] * 10 + d[18] / 10;
  g_sdsData.speed = d[16] * 2;
  g_sdsData.throttlePos = 125UL * (d[19] - 55) / (256 - 55);
  g_sdsData.mapKpa = (uint8_t)(d[20] * 4 * 0.136f);
  g_sdsData.coolantTemp = (d[21] - 48) / 1.6;
  g_sdsData.intakeAirTemp = (d[22] - 48) / 1.6;
  g_sdsData.batteryVolt = (uint8_t)(d[24] * 100 / 126);
  g_sdsData.gearPos = d[26];
  g_sdsData.stps = d[46] / 2.55;
  g_sdsData.clutchIn = (d[52] != 0);
  return true;
}
