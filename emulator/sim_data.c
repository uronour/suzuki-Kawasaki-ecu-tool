#include "sim_data.h"
#include <math.h>
#include <stdlib.h>

SDSData g_sdsData = {0};

static uint32_t g_simTime = 0;
static float g_idleRpm = 1200.0f;
static float g_idlePhase = 0.0f;
static uint8_t g_throttleHeld = 0;
static float g_targetRpm = 1200.0f;
static float g_currentRpm = 1200.0f;
static float g_vehicleSpeed = 0.0f;
static uint8_t g_currentGear = 0;
static float g_gearRatios[7] = {0, 2.545f, 1.778f, 1.364f, 1.111f, 0.958f, 0.852f};
static float g_finalDrive = 2.588f;
static float g_wheelCircumference = 1.95f;

void SimData_Init(void)
{
    g_sdsData.rpm = 1200;
    g_sdsData.speed = 0;
    g_sdsData.coolantTemp = 90;
    g_sdsData.intakeAirTemp = 25;
    g_sdsData.throttlePos = 0;
    g_sdsData.batteryVolt = 142;  // 14.2V * 10
    g_sdsData.gearPos = 0;
    g_sdsData.mapKpa = 35;
    g_sdsData.o2Sensor = 450;
    g_sdsData.injectorPulse = 25;
    g_sdsData.ignitionTiming = 10;
    g_sdsData.iacStep = 35;
    g_sdsData.fanOn = 0;
    g_sdsData.sidestandDown = 1;
    g_sdsData.clutchIn = 0;
}

void SimData_Update(void)
{
    g_simTime++;

    // Handle throttle input simulation
    if (g_throttleHeld) {
        g_targetRpm = 1200.0f + (rand() % 1000) / 1000.0f * 12800.0f;  // 1200-14000 RPM
        g_targetRpm = fminf(g_targetRpm, 14000.0f);
    } else {
        // Return to idle with slight variation
        g_idlePhase += 0.02f;
        g_idleRpm = 1200.0f + sinf(g_idlePhase) * 50.0f + (rand() % 100) / 100.0f * 30.0f - 15.0f;
        g_targetRpm = g_idleRpm;
    }

    // Smooth RPM changes (engine inertia simulation)
    float rpmDiff = g_targetRpm - g_currentRpm;
    float accelRate = (g_throttleHeld) ? 800.0f : 300.0f;  // RPM per second
    float maxChange = accelRate / 60.0f;  // per frame at 60fps
    if (fabsf(rpmDiff) > maxChange) {
        g_currentRpm += (rpmDiff > 0) ? maxChange : -maxChange;
    } else {
        g_currentRpm = g_targetRpm;
    }
    g_sdsData.rpm = (uint32_t)g_currentRpm;

    // Speed calculation based on RPM and gear
    if (g_currentGear > 0 && g_sdsData.clutchIn == 0) {
        float wheelRpm = g_currentRpm / (g_gearRatios[g_currentGear] * g_finalDrive);
        g_vehicleSpeed = wheelRpm * g_wheelCircumference * 60.0f / 1000.0f;  // km/h
        // Add some load effect
        if (g_vehicleSpeed > 280) g_vehicleSpeed = 280;
    } else {
        // Coasting or neutral
        g_vehicleSpeed *= 0.995f;
        if (g_vehicleSpeed < 0.5f) g_vehicleSpeed = 0;
    }
    g_sdsData.speed = (uint32_t)(g_vehicleSpeed * 100);  // stored as km/h * 100

    // Coolant temp - slow warmup
    if (g_sdsData.coolantTemp < 90) {
        if (g_simTime % 300 == 0) g_sdsData.coolantTemp++;
    } else {
        // Thermostat cycling
        if (g_simTime % 600 < 300) g_sdsData.coolantTemp = 90;
        else g_sdsData.coolantTemp = 92;
    }

    // Intake air temp - slight variation
    g_sdsData.intakeAirTemp = 25 + (rand() % 10) - 5;

    // Throttle position
    if (g_throttleHeld) {
        g_sdsData.throttlePos = 30 + (rand() % 70);  // 30-100%
    } else {
        g_sdsData.throttlePos = (rand() % 5);  // 0-5% at idle
    }

    // Battery voltage - slight variation with RPM
    float baseVolt = 14.2f;
    if (g_sdsData.rpm < 2000) baseVolt = 12.8f + (g_sdsData.rpm / 2000.0f) * 1.4f;
    g_sdsData.batteryVolt = (uint32_t)(baseVolt * 10 + (rand() % 20 - 10) / 10.0f);

    // MAP - inverse of throttle roughly
    g_sdsData.mapKpa = 100 - (g_sdsData.throttlePos * 60 / 100) + (rand() % 10);

    // O2 sensor - oscillating around stoich
    g_sdsData.o2Sensor = 400 + (int)(sinf(g_simTime * 0.1f) * 300) + (rand() % 100 - 50);
    if (g_sdsData.o2Sensor > 800) g_sdsData.o2Sensor = 800;
    if (g_sdsData.o2Sensor < 100) g_sdsData.o2Sensor = 100;

    // Injector pulse width
    g_sdsData.injectorPulse = 20 + (g_sdsData.throttlePos * 80 / 100) + (rand() % 10);

    // Ignition timing
    g_sdsData.ignitionTiming = 5 + (g_sdsData.rpm / 1000) + (rand() % 5);

    // IAC steps
    g_sdsData.iacStep = 30 + (rand() % 15);

    // Fan
    g_sdsData.fanOn = (g_sdsData.coolantTemp > 95) ? 1 : 0;

    // Clutch - random for demo
    if (g_simTime % 200 == 0) {
        g_sdsData.clutchIn = rand() % 2;
    }
}

void SimData_SetThrottle(uint8_t held)
{
    g_throttleHeld = held;
}

void SimData_SetGear(uint8_t gear)
{
    if (gear <= 6) g_currentGear = gear;
}

void SimData_ShiftUp(void)
{
    if (g_currentGear < 6) g_currentGear++;
}

void SimData_ShiftDown(void)
{
    if (g_currentGear > 0) g_currentGear--;
}

void SimData_ToggleClutch(void)
{
    g_sdsData.clutchIn = !g_sdsData.clutchIn;
}