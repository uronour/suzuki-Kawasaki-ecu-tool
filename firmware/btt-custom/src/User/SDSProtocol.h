#ifndef _SDS_PROTOCOL_H_
#define _SDS_PROTOCOL_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

typedef enum {
  BRAND_SUZUKI,
  BRAND_KAWASAKI
} ECU_Brand;

void SDS_SetBrand(ECU_Brand brand);
ECU_Brand SDS_GetBrand(void);

#define SDS_ECU_ADDR      0x12
#define KDS_ECU_ADDR      0x11
#define SDS_TESTER_ADDR   0xF1
#define SDS_BAUD          10400
#define SDS_KEEPALIVE_MS  900
#define SDS_TIMEOUT_MS    500
#define SDS_RX_BUF_SIZE   128
#define SDS_TX_BUF_SIZE   16

#define SDS_C2F(x)        ((x) * 9 / 5 + 32)
#define SDS_MPH(x)        ((x) * 1000UL / 1609UL)

#define SDS_MEM_ADDR_LEN  4
#define SDS_MEM_MAX_BLOCK 64
#define SDS_BIN_BUF_SIZE  1024

typedef struct {
  uint16_t rpm;
  uint8_t  coolantTemp;
  uint8_t  intakeAirTemp;
  uint8_t  throttlePos;
  uint16_t batteryVolt;     // tenths of a volt: 142 = 14.2V
  uint8_t  gearPos;
  uint16_t speed;           // km/h * 100 ??
  uint8_t  mapKpa;
  uint8_t  o2Sensor;
  uint8_t  stps;
  uint8_t  ignitionTiming;
  uint8_t  injectorPulse;
  uint8_t  iacStep;
  uint8_t  baroKpa;         // barometric pressure
  uint16_t injectorPW[4];   // individual injector pulse widths *10 (ms*10)
  uint8_t  pairValve;       // PAIR solenoid
  bool     clutchIn;
  bool     neutral;         // neutral switch
  bool     fanOn;
  bool     sidestandDown;
} SDS_Data;

typedef struct {
  uint32_t ecuRawId[4];     // ECU identification dwords
  uint8_t  vin[17];         // VIN string
  uint8_t  vinLen;
  uint32_t flashSize;       // flash size in bytes (0 = unknown)
  uint32_t calOffset;       // calibration region offset
  uint32_t calSize;         // calibration region size
} SDS_EcuInfo;

typedef enum {
  SDS_IDLE,
  SDS_INIT_WAIT_HIGH,
  SDS_INIT_LOW,
  SDS_INIT_HIGH,
  SDS_ACTIVE,
  SDS_ERROR
} SDS_State;

typedef struct {
  uint8_t  data[SDS_RX_BUF_SIZE];
  uint16_t len;
  uint8_t  cmd;
} SDS_Response;

extern SDS_Data g_sdsData;
extern SDS_EcuInfo g_sdsEcuInfo;
extern volatile SDS_State g_sdsState;
extern uint32_t g_sdsLastRxMs;

bool     SDS_Init(void);
void     SDS_PollSensorData(void);
bool     SDS_SendRequest(uint8_t requestId, uint8_t *payload, uint8_t len);
bool     SDS_ReadResponse(SDS_Response *resp);
void     SDS_SendKeepAlive(void);
bool     SDS_EnterDealerMode(void);
uint16_t SDS_GetDTCs(uint8_t *dtcBuf, uint8_t maxLen);
bool     SDS_ClearDTCs(void);
void     SDS_SetKLineUART(uint8_t port);
bool     SDS_ParseSensorData(const SDS_Response *resp);

// Extended protocol
bool     SDS_StopCommunication(void);
bool     SDS_ReadEcuInfo(SDS_EcuInfo *info);
uint16_t SDS_ReadMemoryByAddress(uint32_t addr, uint8_t len, uint8_t *buf);
bool     SDS_WriteMemoryByAddress(uint32_t addr, uint8_t len, const uint8_t *buf);
bool     SDS_RequestDownload(uint32_t addr, uint32_t size, uint16_t *maxBlock);
uint16_t SDS_TransferData(uint8_t seq, const uint8_t *data, uint16_t len);
bool     SDS_RequestTransferExit(void);
uint32_t SDS_DumpCalibration(uint8_t *buf, uint32_t maxLen, uint32_t baseAddr, uint32_t size);
bool     SDS_IsConnected(void);

#ifdef __cplusplus
}
#endif

#endif
