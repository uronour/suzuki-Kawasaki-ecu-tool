#ifndef _SIM_DATA_H_
#define _SIM_DATA_H_

#include <stdint.h>

typedef struct {
    uint32_t rpm;
    uint32_t speed;
    uint32_t coolantTemp;
    uint32_t intakeAirTemp;
    uint32_t throttlePos;
    uint32_t batteryVolt;
    uint32_t gearPos;
    uint32_t mapKpa;
    uint32_t o2Sensor;
    uint32_t injectorPulse;
    uint32_t ignitionTiming;
    uint32_t iacStep;
    uint32_t fanOn;
    uint32_t sidestandDown;
    uint32_t clutchIn;
} SDSData;

extern SDSData g_sdsData;

void SimData_Init(void);
void SimData_Update(void);
void SimData_ShiftUp(void);
void SimData_ShiftDown(void);
void SimData_SetThrottle(uint8_t held);
void SimData_ToggleClutch(void);

#endif