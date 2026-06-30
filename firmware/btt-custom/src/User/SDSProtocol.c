#include "SDSProtocol.h"
#include "includes.h"
#include <string.h>

SDS_Data g_sdsData;
SDS_EcuInfo g_sdsEcuInfo;
volatile SDS_State g_sdsState = SDS_IDLE;
uint32_t g_sdsLastRxMs = 0;

static uint8_t g_sdsKLinePort = _USART3;
static ECU_Brand g_ecuBrand = BRAND_SUZUKI;

void SDS_SetBrand(ECU_Brand brand)
{
  g_ecuBrand = brand;
}

ECU_Brand SDS_GetBrand(void)
{
  return g_ecuBrand;
}

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
  uint8_t frame[64];
  uint8_t idx = 0;
  uint8_t ecuAddr = (g_ecuBrand == BRAND_KAWASAKI) ? KDS_ECU_ADDR : SDS_ECU_ADDR;

  frame[idx++] = 0x80 | pidLen;
  frame[idx++] = ecuAddr;
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

  // Verify response matches expected tester address (resp[1]) and ECU address (resp[2])
  uint8_t expectedTarget = SDS_TESTER_ADDR;
  uint8_t expectedSource = (g_ecuBrand == BRAND_KAWASAKI) ? KDS_ECU_ADDR : SDS_ECU_ADDR;

  if (resp[0] != 0x80 || resp[1] != expectedTarget || resp[2] != expectedSource || resp[4] != 0xC1)
  {
    g_sdsState = SDS_ERROR;
    KLine_SetConnected(false);
    return false;
  }

  if (g_ecuBrand == BRAND_KAWASAKI) {
    // Kawasaki needs StartDiagnosticSession (0x10 0x80)
    uint8_t pidDiag[] = { 0x10, 0x80 };
    SDS_SendFrame(pidDiag, sizeof(pidDiag));
    if (!SDS_WaitResponse(resp, SDS_RX_BUF_SIZE, SDS_TIMEOUT_MS)) return false;
  } else {
    // Suzuki needs AccessTimingParameter (0x83 0x02)
    uint8_t pidATP[] = { 0x83, 0x02 };
    SDS_SendFrame(pidATP, sizeof(pidATP));
    if (!SDS_WaitResponse(resp, SDS_RX_BUF_SIZE, SDS_TIMEOUT_MS)) return false;
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

  if (g_ecuBrand == BRAND_KAWASAKI) {
    // KDS PIDs for ZX10R 2004
    // RPM: 0x21 0x09
    uint8_t pidRpm[] = { 0x21, 0x09 };
    SDS_SendFrame(pidRpm, sizeof(pidRpm));
    uint8_t resp[SDS_RX_BUF_SIZE];
    if (SDS_WaitResponse(resp, SDS_RX_BUF_SIZE, 200)) {
        g_sdsData.rpm = resp[5] * 100; // Simplified parsing for now
    }

    // Speed: 0x21 0x0C
    uint8_t pidSpd[] = { 0x21, 0x0C };
    SDS_SendFrame(pidSpd, sizeof(pidSpd));
    if (SDS_WaitResponse(resp, SDS_RX_BUF_SIZE, 200)) {
        g_sdsData.speed = resp[5] * 100;
    }

    // TPS: 0x21 0x04
    uint8_t pidTps[] = { 0x21, 0x04 };
    SDS_SendFrame(pidTps, sizeof(pidTps));
    if (SDS_WaitResponse(resp, SDS_RX_BUF_SIZE, 200)) {
        g_sdsData.throttlePos = resp[5];
    }

    // ECT: 0x21 0x06
    uint8_t pidEct[] = { 0x21, 0x06 };
    SDS_SendFrame(pidEct, sizeof(pidEct));
    if (SDS_WaitResponse(resp, SDS_RX_BUF_SIZE, 200)) {
        g_sdsData.coolantTemp = resp[5];
    }

    // Gear: 0x21 0x0B
    uint8_t pidGear[] = { 0x21, 0x0B };
    SDS_SendFrame(pidGear, sizeof(pidGear));
    if (SDS_WaitResponse(resp, SDS_RX_BUF_SIZE, 200)) {
        g_sdsData.gearPos = resp[5];
    }

    g_sdsLastRxMs = OS_GetTimeMs();
  } else {
    // Standard Suzuki SDS Poll
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
  g_sdsData.batteryVolt = (uint16_t)(d[24] * 100 / 126);
  g_sdsData.gearPos = d[26];

  // STPS, ignition timing, injector pulse, IAC
  g_sdsData.stps = d[46] / 2.55;
  g_sdsData.ignitionTiming = d[47];
  g_sdsData.injectorPulse = d[30];           // approximate single injector
  g_sdsData.iacStep = d[29];                 // ISC valve position

  // Barometric pressure (KWP2000 offset: raw[27])
  if (resp->len > 28)
    g_sdsData.baroKpa = (uint8_t)(d[27] * 4 * 0.136f);

  // Individual injector pulse widths (KWP2000 offset: raw[31-38])
  if (resp->len > 39) {
    g_sdsData.injectorPW[0] = d[31] * 256 + d[32];
    g_sdsData.injectorPW[1] = d[33] * 256 + d[34];
    g_sdsData.injectorPW[2] = d[35] * 256 + d[36];
    g_sdsData.injectorPW[3] = d[37] * 256 + d[38];
  }

  // PAIR valve (KWP2000 offset: raw[51])
  if (resp->len > 52)
    g_sdsData.pairValve = d[51];

  // Clutch (KWP2000 offset: raw[52])
  if (resp->len > 53)
    g_sdsData.clutchIn = (d[52] != 0);

  // Neutral (offset tentative: raw[53])
  if (resp->len > 54)
    g_sdsData.neutral = (d[53] != 0);

  // O2 sensor (offset known from KWP2000 comment: raw[25])
  if (resp->len > 26)
    g_sdsData.o2Sensor = d[25];

  // Fan, sidestand not available from SDS data stream (GPIO pins)
  return true;
}

// ---- Extended protocol operations ----

bool SDS_StopCommunication(void)
{
  if (g_sdsState != SDS_ACTIVE)
    return true;  // already stopped

  uint8_t pid[] = { 0x82 };
  SDS_SendFrame(pid, sizeof(pid));

  uint8_t resp[SDS_RX_BUF_SIZE];
  if (!SDS_WaitResponse(resp, SDS_RX_BUF_SIZE, SDS_TIMEOUT_MS))
  {
    g_sdsState = SDS_IDLE;
    KLine_SetConnected(false);
    return false;
  }

  bool ok = (resp[4] == 0xC2);
  g_sdsState = SDS_IDLE;
  KLine_SetConnected(false);
  return ok;
}

bool SDS_ReadEcuInfo(SDS_EcuInfo *info)
{
  if (!info) return false;
  memset(info, 0, sizeof(*info));

  if (g_sdsState != SDS_ACTIVE) return false;

  // Try ReadDataByCommonIdentifier (0x22) for ECU identification
  // DID 0xF190 = ECU serial number (standard KWP2000)
  uint8_t pid[] = { 0x22, 0xF1, 0x90 };
  SDS_SendFrame(pid, sizeof(pid));

  uint8_t resp[SDS_RX_BUF_SIZE];
  if (SDS_WaitResponse(resp, SDS_RX_BUF_SIZE, SDS_TIMEOUT_MS) && resp[4] == 0x62)
  {
    uint8_t dlen = resp[3];
    for (int i = 0; i < 4 && (i * 4 + 5 + 4) <= dlen + 4; i++)
      info->ecuRawId[i] = (uint32_t)resp[5 + i * 4] << 24 | (uint32_t)resp[6 + i * 4] << 16
                         | (uint32_t)resp[7 + i * 4] << 8 | resp[8 + i * 4];
  }

  // Try to read VIN via ReadDataByLocalIdentifier (0x21, 0x00 = VIN)
  uint8_t pidVin[] = { 0x21, 0x00 };
  SDS_SendFrame(pidVin, sizeof(pidVin));

  if (SDS_WaitResponse(resp, SDS_RX_BUF_SIZE, SDS_TIMEOUT_MS) && resp[4] == 0x61)
  {
    uint8_t dlen = resp[3];
    uint8_t vlen = dlen - 1;  // subtract SID byte
    if (vlen > 16) vlen = 16;
    memcpy(info->vin, &resp[5], vlen);
    info->vin[vlen] = '\0';
    info->vinLen = vlen;
  }

  // Calibration region guess for 2004 GSX-R1000
  // These are ECU-specific; store placeholder values
  info->flashSize = 524288;   // 512KB typical for 2004
  info->calOffset = 0x8000;
  info->calSize   = 0x18000;  // 96KB calibration region

  return true;
}

uint16_t SDS_ReadMemoryByAddress(uint32_t addr, uint8_t len, uint8_t *buf)
{
  if (!buf || len == 0 || len > SDS_MEM_MAX_BLOCK) return 0;
  if (g_sdsState != SDS_ACTIVE) return 0;

  // KWP2000 ReadMemoryByAddress (0x23) with 1-byte ALFID (0x11 = 1-byte addr, 1-byte len)
  // For 32-bit MCU use ALFID 0x44 (4-byte addr, 4-byte len)
  // But Suzuki may use manufacturer-specific format
  uint8_t pid[7];
  pid[0] = 0x23;
  pid[1] = 0x44;     // ALFID: 4 bytes address, 4 bytes length
  pid[2] = (uint8_t)(addr >> 24);
  pid[3] = (uint8_t)(addr >> 16);
  pid[4] = (uint8_t)(addr >> 8);
  pid[5] = (uint8_t)(addr);
  pid[6] = len;

  SDS_SendFrame(pid, sizeof(pid));

  uint8_t resp[SDS_RX_BUF_SIZE];
  if (!SDS_WaitResponse(resp, SDS_RX_BUF_SIZE, SDS_TIMEOUT_MS))
    return 0;

  if (resp[4] != 0x63)
    return 0;

  // Response: 0x63 [data...], data is at resp[5]
  uint8_t dataLen = resp[3] - 1;  // subtract SID byte
  if (dataLen > len) dataLen = len;
  memcpy(buf, &resp[5], dataLen);
  return dataLen;
}

bool SDS_WriteMemoryByAddress(uint32_t addr, uint8_t len, const uint8_t *buf)
{
  if (!buf || len == 0) return false;
  if (g_sdsState != SDS_ACTIVE) return false;

  // WriteMemoryByAddress (0x3D) with ALFID 0x44
  uint8_t pid[7 + len];
  pid[0] = 0x3D;
  pid[1] = 0x44;
  pid[2] = (uint8_t)(addr >> 24);
  pid[3] = (uint8_t)(addr >> 16);
  pid[4] = (uint8_t)(addr >> 8);
  pid[5] = (uint8_t)(addr);
  memcpy(&pid[6], buf, len);

  SDS_SendFrame(pid, 6 + len);

  uint8_t resp[SDS_RX_BUF_SIZE];
  if (!SDS_WaitResponse(resp, SDS_RX_BUF_SIZE, SDS_TIMEOUT_MS))
    return false;

  return (resp[4] == 0x7D);
}

bool SDS_RequestDownload(uint32_t addr, uint32_t size, uint16_t *maxBlock)
{
  if (g_sdsState != SDS_ACTIVE) return false;

  // RequestDownload (0x34): opens data transfer TO ECU
  uint8_t pid[11];
  pid[0] = 0x34;
  pid[1] = 0x00;        // DFI: no compression, no encryption
  pid[2] = 0x44;        // ALFID: 4-byte addr, 4-byte size
  pid[3] = (uint8_t)(addr >> 24);
  pid[4] = (uint8_t)(addr >> 16);
  pid[5] = (uint8_t)(addr >> 8);
  pid[6] = (uint8_t)(addr);
  pid[7] = (uint8_t)(size >> 24);
  pid[8] = (uint8_t)(size >> 16);
  pid[9] = (uint8_t)(size >> 8);
  pid[10] = (uint8_t)(size);

  SDS_SendFrame(pid, sizeof(pid));

  uint8_t resp[SDS_RX_BUF_SIZE];
  if (!SDS_WaitResponse(resp, SDS_RX_BUF_SIZE, 2000))
    return false;

  if (resp[4] != 0x74)
    return false;

  // Response: 0x74 [lenFormatId] [maxBlockLen...]
  if (resp[3] >= 3 && maxBlock)
  {
    uint8_t lfi = resp[5];                  // length format identifier
    uint8_t lfLen = (lfi >> 4) & 0x0F;      // how many bytes for max block length
    if (lfLen == 0) lfLen = 1;
    uint32_t mb = 0;
    for (int i = 0; i < lfLen && i < 4; i++)
      mb = (mb << 8) | resp[6 + i];
    *maxBlock = (uint16_t)(mb > 0xFFFF ? 0xFFFF : mb);
  }

  return true;
}

uint16_t SDS_TransferData(uint8_t seq, const uint8_t *data, uint16_t len)
{
  if (!data || len == 0) return 0;
  if (g_sdsState != SDS_ACTIVE) return 0;
  if (len > SDS_MEM_MAX_BLOCK) len = SDS_MEM_MAX_BLOCK;

  // TransferData (0x36): block sequence counter + data
  uint8_t pid[2 + len];
  pid[0] = 0x36;
  pid[1] = seq;         // block sequence counter
  memcpy(&pid[2], data, len);

  SDS_SendFrame(pid, 2 + len);

  uint8_t resp[SDS_RX_BUF_SIZE];
  if (!SDS_WaitResponse(resp, SDS_RX_BUF_SIZE, 2000))
    return 0;

  if (resp[4] != 0x76)
    return 0;

  // Return number of bytes accepted (from response data)
  return len;
}

bool SDS_RequestTransferExit(void)
{
  if (g_sdsState != SDS_ACTIVE) return false;

  uint8_t pid[] = { 0x37 };
  SDS_SendFrame(pid, sizeof(pid));

  uint8_t resp[SDS_RX_BUF_SIZE];
  if (!SDS_WaitResponse(resp, SDS_RX_BUF_SIZE, 2000))
    return false;

  return (resp[4] == 0x77);
}

uint32_t SDS_DumpCalibration(uint8_t *buf, uint32_t maxLen, uint32_t baseAddr, uint32_t size)
{
  if (!buf || maxLen == 0 || size == 0) return 0;
  if (g_sdsState != SDS_ACTIVE) return 0;

  uint32_t total = 0;
  uint32_t remaining = size;
  uint32_t addr = baseAddr;

  while (remaining > 0 && total < maxLen)
  {
    uint8_t chunkLen = (remaining > SDS_MEM_MAX_BLOCK) ? SDS_MEM_MAX_BLOCK : (uint8_t)remaining;
    uint16_t got = SDS_ReadMemoryByAddress(addr, chunkLen, &buf[total]);
    if (got == 0)
      break;
    total += got;
    addr += got;
    remaining -= got;
  }

  return total;
}

bool SDS_IsConnected(void)
{
  return (g_sdsState == SDS_ACTIVE) && KLine_IsConnected();
}
