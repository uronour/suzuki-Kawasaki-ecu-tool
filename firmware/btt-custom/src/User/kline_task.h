#ifndef _KLINE_TASK_H_
#define _KLINE_TASK_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

#define KLINE_UART_PORT  _USART2
#define KLINE_BAUD       10400
#define KLINE_TX_PIN     PA2
#define KLINE_RX_PIN     PA3
#define KLINE_AF         GPIO_AF_USART2

#define KLINE_DEALER_GPIO_PORT  GPIOC
#define KLINE_DEALER_GPIO_PIN   GPIO_Pin_0

void KLine_Init(void);
bool KLine_FastInit(void);
void KLine_SendByte(uint8_t b);
void KLine_SendBuf(const uint8_t *buf, uint16_t len);
uint8_t KLine_ReadByte(void);
uint16_t KLine_Available(void);
uint16_t KLine_ReadBuf(uint8_t *buf, uint16_t maxLen, uint32_t timeoutMs);
void KLine_SetTXHigh(void);
void KLine_SetTXLow(void);
void KLine_SetTXtoUART(void);
void KLine_SetDealerMode(bool active);
bool KLine_GetDealerMode(void);
bool KLine_IsConnected(void);
void KLine_SetConnected(bool connected);
void KLine_Reset(void);
uint8_t KLine_LoopbackTest(void);

#ifdef __cplusplus
}
#endif

#endif
