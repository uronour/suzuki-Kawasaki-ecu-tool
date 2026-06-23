#include "kline_task.h"
#include "includes.h"

static uint32_t g_lastKeepAlive = 0;
static uint32_t g_lastPoll = 0;
static volatile bool g_connected = false;
static volatile bool g_dealerMode = false;

void KLine_SetTXHigh(void)
{
  GPIO_SetLevel(KLINE_TX_PIN, 1);
}

void KLine_SetTXLow(void)
{
  GPIO_InitSet(KLINE_TX_PIN, MGPIO_MODE_OUT_PP, 0);
  GPIO_SetLevel(KLINE_TX_PIN, 0);
}

void KLine_SetTXtoUART(void)
{
  GPIO_InitSet(KLINE_TX_PIN, MGPIO_MODE_AF_PP, KLINE_AF);
  GPIO_InitSet(KLINE_RX_PIN, MGPIO_MODE_AF_PP, KLINE_AF);
}

bool KLine_FastInit(void)
{
  KLine_SetTXHigh();
  Delay_ms(1000);
  KLine_SetTXLow();
  Delay_ms(25);
  KLine_SetTXHigh();
  Delay_ms(25);

  KLine_SetTXtoUART();
  UART_Config(KLINE_UART_PORT, KLINE_BAUD, 0, false);
  return true;
}

void KLine_Init(void)
{
  GPIO_InitSet(KLINE_DEALER_GPIO_PIN, MGPIO_MODE_OUT_PP, 0);
  GPIO_SetLevel(KLINE_DEALER_GPIO_PIN, 0);

  if (KLine_FastInit())
  {
    g_connected = true;
  }
}

void KLine_SendByte(uint8_t b)
{
  UART_Write(KLINE_UART_PORT, b);
}

void KLine_SendBuf(const uint8_t *buf, uint16_t len)
{
  for (uint16_t i = 0; i < len; i++)
    UART_Write(KLINE_UART_PORT, buf[i]);
}

uint8_t KLine_ReadByte(void)
{
  while ((USART3->SR & USART_SR_RXNE) == 0);
  return USART3->DR;
}

uint16_t KLine_Available(void)
{
  return (USART3->SR & USART_SR_RXNE) ? 1 : 0;
}

uint16_t KLine_ReadBuf(uint8_t *buf, uint16_t maxLen, uint32_t timeoutMs)
{
  uint16_t idx = 0;
  uint32_t start = OS_GetTimeMs();
  while (OS_GetTimeMs() - start < timeoutMs)
  {
    if (KLine_Available())
      buf[idx++] = USART3->DR;
    if (idx >= maxLen)
      break;
    Delay_us(100);
  }
  return idx;
}

void KLine_SetDealerMode(bool active)
{
  g_dealerMode = active;
  GPIO_SetLevel(KLINE_DEALER_GPIO_PIN, active ? 1 : 0);
}

bool KLine_GetDealerMode(void)
{
  return g_dealerMode;
}

bool KLine_IsConnected(void)
{
  return g_connected;
}

void KLine_SetConnected(bool connected)
{
  g_connected = connected;
}

void KLine_Reset(void)
{
  g_connected = false;
  UART_DeConfig(KLINE_UART_PORT);
  Delay_ms(100);
  KLine_Init();
}
