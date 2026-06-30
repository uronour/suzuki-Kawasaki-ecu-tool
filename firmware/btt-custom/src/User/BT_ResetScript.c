#include "BT_ResetScript.h"
#include "includes.h"
#include <stdio.h>
#include <string.h>

static uint16_t g_resetLineY = 10;

void BT_ConsolePrint(const char *msg)
{
  GFX_DrawString(10, g_resetLineY, msg, COLOR_GREEN, COLOR_BLACK);
  g_resetLineY += 20;
  if (g_resetLineY > 300) {
    g_resetLineY = 10;
    LCD_Fill(0, 0, LCD_WIDTH, LCD_HEIGHT, COLOR_BLACK);
  }
}

bool BT_WaitForOK(uint8_t port, uint32_t timeoutMs)
{
  uint32_t start = OS_GetTimeMs();
  char rxBuf[64];
  uint8_t rxIdx = 0;

  while(OS_GetTimeMs() - start < timeoutMs) {
    while (Serial_GetWritingIndexRX(port) != Serial_GetReadingIndexRX(port)) {
      char c = dmaL1DataRX[port].cache[Serial_GetReadingIndexRX(port)];
      dmaL1DataRX[port].rIndex = (dmaL1DataRX[port].rIndex + 1) % dmaL1DataRX[port].cacheSize;

      if (c == '\r' || c == '\n') {
        if (rxIdx > 0) {
          rxBuf[rxIdx] = '\0';
          BT_ConsolePrint(rxBuf);
          if (strstr(rxBuf, "OK")) return true;
          rxIdx = 0;
        }
      } else if (rxIdx < 63 && c >= 32) {
        rxBuf[rxIdx++] = c;
      }
    }
  }
  return false;
}

void BT_HC05_ResetScript(void)
{
  LCD_Fill(0, 0, LCD_WIDTH, LCD_HEIGHT, COLOR_BLACK);
  BT_ConsolePrint("SYSTEM DIAGNOSTIC MODE...");
  BT_ConsolePrint("--------------------------------");

  // --- K-LINE LOOPBACK TEST ---
  BT_ConsolePrint("K-LINE HARDWARE TEST (USART2)");
  BT_ConsolePrint("Connect PA2 to PA3 now...");

  uint8_t klineResult = 0;
  while (klineResult < 8) {
    klineResult = KLine_LoopbackTest();
    char klMsg[40];
    snprintf(klMsg, sizeof(klMsg), "Result: %d/8 passed...", klineResult);
    BT_ConsolePrint(klMsg);

    if (klineResult == 8) {
      BT_ConsolePrint(">> K-LINE HARDWARE OK!");
      break;
    }

    BT_ConsolePrint("Retrying in 2s...");
    Delay_ms(2000);
  }
  BT_ConsolePrint("--------------------------------");
  Delay_ms(1000);

  // --- BLUETOOTH SCAN & RESET ---
  BT_ConsolePrint("SEARCHING FOR HC-05...");
  BT_ConsolePrint("Hold BT button on HC-05");

  uint8_t ports[] = {_USART3, _UART4};
  uint32_t bauds[] = {38400, 9600};
  uint8_t winPort = 0xFF;

  // Buffers for detection
  char rxBuf[64];
  uint8_t rxIdx = 0;

  while(winPort == 0xFF) {
    for (int p = 0; p < 2; p++) {
      for (int b = 0; b < 2; b++) {
        uint8_t port = ports[p];
        uint32_t baud = bauds[b];

        char msg[40];
        snprintf(msg, sizeof(msg), "Scan U%d @ %lu...", (port == _USART3 ? 3 : 4), baud);
        BT_ConsolePrint(msg);

        UART_Config(port, baud, 0, false);
        Serial_Config(port, 128, 128, baud);

        UART_Puts(port, (uint8_t *)"AT\r\n");

        uint32_t start = OS_GetTimeMs();
        bool portFound = false;
        while(OS_GetTimeMs() - start < 1000) {
           while (Serial_GetWritingIndexRX(port) != Serial_GetReadingIndexRX(port)) {
              char c = dmaL1DataRX[port].cache[Serial_GetReadingIndexRX(port)];
              dmaL1DataRX[port].rIndex = (dmaL1DataRX[port].rIndex + 1) % dmaL1DataRX[port].cacheSize;
              if (c == '\r' || c == '\n') {
                if (rxIdx > 0) {
                  rxBuf[rxIdx] = '\0';
                  BT_ConsolePrint(rxBuf);
                  if (strstr(rxBuf, "OK")) {
                     winPort = port;
                     portFound = true;
                     break;
                  }
                  rxIdx = 0;
                }
              } else if (rxIdx < 63 && c >= 32) {
                rxBuf[rxIdx++] = c;
              }
           }
           if (portFound) break;
        }

        if (portFound) break;
        Serial_DeConfig(port);
      }
      if (winPort != 0xFF) break;
    }
  }

  char winMsg[50];
  snprintf(winMsg, sizeof(winMsg), "FOUND HC-05 on UART%d!", (winPort == _USART3 ? 3 : 4));
  BT_ConsolePrint(winMsg);
  BT_ConsolePrint("Proceeding with reset...");

  BT_ConsolePrint("STEP 1: Factory Reset");
  UART_Puts(winPort, (uint8_t *)"AT+ORGL\r\n");
  BT_WaitForOK(winPort, 2000);

  BT_ConsolePrint("STEP 3: Set Name");
  UART_Puts(winPort, (uint8_t *)"AT+NAME=SuzukiECU\r\n");
  BT_WaitForOK(winPort, 1000);

  BT_ConsolePrint("STEP 4: Set Password");
  UART_Puts(winPort, (uint8_t *)"AT+PSWD=\"1234\"\r\n");
  BT_WaitForOK(winPort, 1000);

  BT_ConsolePrint("STEP 5: Set Baud 115200");
  UART_Puts(winPort, (uint8_t *)"AT+UART=115200,0,0\r\n");
  BT_WaitForOK(winPort, 1000);

  BT_ConsolePrint("STEP 6: Restart HC-05");
  UART_Puts(winPort, (uint8_t *)"AT+RESET\r\n");

  BT_ConsolePrint("--------------------------------");
  BT_ConsolePrint("SETUP COMPLETE!");

  char finalWin[40];
  snprintf(finalWin, sizeof(finalWin), "USE PORT: UART%d", (winPort == _USART3 ? 3 : 4));
  GFX_DrawStringCenterScaled(140, finalWin, COLOR_YELLOW, COLOR_BLACK, 2);
  GFX_DrawStringCenterScaled(180, "SUCCESS", COLOR_CYAN, COLOR_BLACK, 4);

  BT_ConsolePrint(finalWin);
  BT_ConsolePrint("Restart board now.");

  while(1) Delay_ms(1000);
}
