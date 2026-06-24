#ifndef _STUBS_H_
#define _STUBS_H_

#include <stdint.h>
#include <stdbool.h>
#include "sim_data.h"

#define SDS_KEEPALIVE_MS 900

extern SDSData g_sdsData;

void SDS_Init(void);
void SDS_PollSensorData(void);
void SDS_SendKeepAlive(void);
uint16_t SDS_GetDTCs(uint8_t *buf, uint16_t maxLen);

bool KLine_IsConnected(void);
void KLine_SetConnected(bool connected);

void BT_Init(void);
void BT_Stream(void);
bool BT_IsConnected(void);

void SD_Log_Init(void);
void SD_Log_Tick(void);
bool SD_Log_IsMounted(void);
bool SD_Log_IsActive(void);
void SD_Log_Start(void);
void SD_Log_Stop(void);

#endif