#include "stubs.h"
#include "sim_data.h"
#include "sdl_gfx.h"
#include <stdlib.h>

typedef struct {
    volatile uint32_t ms;
    uint16_t sec;
} OS_COUNTER;

OS_COUNTER os_counter = {0, 0};

static uint32_t g_startTime = 0;

uint32_t OS_GetTimeMs(void)
{
    return SDL_GetTicks() - g_startTime;
}

void Delay_ms(uint32_t ms)
{
    uint32_t start = SDL_GetTicks();
    while (SDL_GetTicks() - start < ms) {
        SDL_Delay(1);
        os_counter.ms = SDL_GetTicks() - g_startTime;
    }
}

void OS_InitTimerMs(void)
{
    g_startTime = SDL_GetTicks();
    os_counter.ms = 0;
    os_counter.sec = 0;
}

static uint8_t g_connected = 0;
static uint8_t g_sdMounted = 1;
static uint8_t g_sdLogging = 0;
static uint8_t g_btConnected = 1;

void SDS_Init(void)
{
    g_connected = 1;
    KLine_SetConnected(1);
}

void SDS_PollSensorData(void)
{
    // Data is updated by SimData_Update()
}

void SDS_SendKeepAlive(void)
{
    // No-op in simulation
}

uint16_t SDS_GetDTCs(uint8_t *buf, uint16_t maxLen)
{
    (void)buf; (void)maxLen;
    return 0;  // No DTCs in simulation
}

bool KLine_IsConnected(void)
{
    return g_connected;
}

void KLine_SetConnected(bool connected)
{
    g_connected = connected;
}

void BT_Init(void)
{
}

void BT_Stream(void)
{
}

bool BT_IsConnected(void)
{
    return g_btConnected;
}

void SD_Log_Init(void)
{
}

void SD_Log_Tick(void)
{
}

bool SD_Log_IsMounted(void)
{
    return g_sdMounted;
}

bool SD_Log_IsActive(void)
{
    return g_sdLogging;
}

void SD_Log_Start(void)
{
    g_sdLogging = 1;
}

void SD_Log_Stop(void)
{
    g_sdLogging = 0;
}