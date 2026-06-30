#include "kline_task.h"
#include "includes.h"

static uint32_t g_lastKeepAlive = 0;
static uint32_t g_lastPoll = 0;
static volatile bool g_connected = false;
static volatile bool g_dealerMode = false;

static USART_TypeDef * const kline_uart[] = {USART1, USART2, USART3, UART4, UART5, USART6};
#define KLINE_UART  kline_uart[KLINE_UART_PORT]

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
  while ((KLINE_UART->SR & USART_SR_RXNE) == 0);
  return KLINE_UART->DR;
}

uint16_t KLine_Available(void)
{
  return (KLINE_UART->SR & USART_SR_RXNE) ? 1 : 0;
}

uint16_t KLine_ReadBuf(uint8_t *buf, uint16_t maxLen, uint32_t timeoutMs)
{
  uint16_t idx = 0;
  uint32_t start = OS_GetTimeMs();
  while (OS_GetTimeMs() - start < timeoutMs)
  {
    if (KLine_Available())
      buf[idx++] = KLINE_UART->DR;
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

uint8_t KLine_LoopbackTest(void)
{
  // 1. Ensure Port is initialized
  UART_Config(KLINE_UART_PORT, 9600, 0, false);

  // 2. Clear any junk from the RX buffer
  volatile uint8_t junk;
  for(int i=0; i<10; i++) {
    if (USART_GetFlagStatus(KLINE_UART, USART_FLAG_RXNE) != RESET)
       junk = USART_ReceiveData(KLINE_UART);
  }

  uint8_t test_pattern[] = {0x55, 0xAA, 0x0F, 0xF0, 0x33, 0xCC, 0x12, 0xED};
  uint8_t passed = 0;

  for (uint8_t i = 0; i < sizeof(test_pattern); i++) {
    // Wait for TX empty
    while (USART_GetFlagStatus(KLINE_UART, USART_FLAG_TXE) == RESET);
    USART_SendData(KLINE_UART, test_pattern[i]);

    // Wait for RX with a generous timeout for 9600 baud
    uint32_t timeout = 400000;
    while (USART_GetFlagStatus(KLINE_UART, USART_FLAG_RXNE) == RESET) {
      if (--timeout == 0) break;
    }

    if (timeout > 0) {
      uint8_t rx = USART_ReceiveData(KLINE_UART);
      if (rx == test_pattern[i]) passed++;
    }
  }

  // 3. Restore ECU Baud rate
  UART_Config(KLINE_UART_PORT, KLINE_BAUD, 0, false);

  return passed;
}
