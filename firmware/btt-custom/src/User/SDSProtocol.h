#ifndef _SDS_PROTOCOL_H_
#define _SDS_PROTOCOL_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

#define SDS_ECU_ADDR      0x12
#define SDS_TESTER_ADDR   0xF1
#define SDS_BAUD          10400
#define SDS_KEEPALIVE_MS  900
#define SDS_TIMEOUT_MS    500
#define SDS_RX_BUF_SIZE   128
#define SDS_TX_BUF_SIZE   16

#define SDS_C2F(x)        ((x) * 9 / 5 + 32)
#define SDS_MPH(x)        ((x) * 1000UL / 1609UL)

typedef struct {
  uint16_t rpm;
  uint8_t  coolantTemp;
  uint8_t  intakeAirTemp;
  uint8_t  throttlePos;
  uint8_t  batteryVolt;
  uint8_t  gearPos;
  uint16_t speed;
  uint8_t  mapKpa;
  uint8_t  o2Sensor;
  uint8_t  stps;
  uint8_t  ignitionTiming;
  uint8_t  injectorPulse;
  uint8_t  iacStep;
  bool     clutchIn;
  bool     fanOn;
  bool     sidestandDown;
} SDS_Data;

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

#ifdef __cplusplus
}
#endif

#endif
