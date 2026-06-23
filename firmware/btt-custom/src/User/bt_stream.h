#ifndef _BT_STREAM_H_
#define _BT_STREAM_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

#define BT_UART_PORT  _USART1
#define BT_BAUD       9600
#define BT_TX_BUF     128
#define BT_RX_BUF     64

void BT_Init(void);
void BT_Stream(void);
void BT_SendLine(const char *fmt, ...);
bool BT_IsConnected(void);
void BT_ProcessCommand(const char *cmd);
void BT_SendJSON(void);

#ifdef __cplusplus
}
#endif

#endif
