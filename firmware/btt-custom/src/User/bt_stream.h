#ifndef _BT_STREAM_H_
#define _BT_STREAM_H_

#include <stdint.h>
#include <stdbool.h>
#include "BT_ResetScript.h"

#define BT_UART_PORT  _UART4
#define BT_BAUD       115200
#define BT_TX_BUF     256
#define BT_RX_BUF     128

void BT_Init(void);
void BT_Stream(void);
void BT_SendLine(const char *fmt, ...);
void BT_SendJSON(void);
void BT_SendInfo(void);
bool BT_IsConnected(void);

#endif
