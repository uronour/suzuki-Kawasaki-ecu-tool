/*
 * sds_esp32_tft.ino — ESP32 + TFT Gauge Cluster Display
 *
 * Standalone on-device dashboard with analog-style gauges.
 * No PC or phone needed — everything on the 2.8" TFT.
 *
 * Requires: TFT_eSPI library by Bodmer
 * Install: PlatformIO lib_deps or Arduino Library Manager
 *
 * Hardware: ESP32 + L9637D + 4N35 + ILI9341 TFT
 * See hardware/esp32_upgrade.md for pinout
 */

#include "SDSProtocol.h"
#include <TFT_eSPI.h>
#include <SPI.h>

// Display
TFT_eSPI tft;
TFT_eSprite spr = TFT_eSprite(&tft);

// K-Line on UART2
HardwareSerial kline(2);
SDSProtocol ecu(&kline, 17, 4); // TX=17, Dealer=4

// Colors
#define BG_COLOR TFT_BLACK
#define TEXT_COLOR TFT_WHITE
#define RPM_COLOR TFT_RED
#define BAR_COLOR TFT_GREEN
#define WARN_COLOR TFT_ORANGE
#define ALERT_COLOR TFT_RED

#define SCREEN_W 320
#define SCREEN_H 240

// Page state
enum Page { PAGE_MAIN, PAGE_GAUGES, PAGE_INJECTORS, PAGE_DTCS };
Page current_page = PAGE_MAIN;

// Touch calibration (adjust for your display)
uint16_t calib[4] = { 320, 320, 240, 240 };

void setup() {
    Serial.begin(115200);

    tft.init();
    tft.setRotation(1); // Landscape
    tft.fillScreen(BG_COLOR);
    tft.setTextColor(TEXT_COLOR, BG_COLOR);

    spr.createSprite(SCREEN_W, SCREEN_H);

    pinMode(4, OUTPUT);
    digitalWrite(4, LOW);

    draw_splash();
    delay(2000);

    Serial.println(F("Initializing K-Line..."));
    if (ecu.begin()) {
        Serial.println(F("ECU connected!"));
        ecu.set_dealer_mode(true);
    } else {
        draw_status("ECU offline - check wiring", ALERT_COLOR);
    }
}

void loop() {
    static unsigned long last_sensor = 0;

    // Poll sensors at ~10Hz
    if (ecu.is_connected() && millis() - last_sensor > 100) {
        last_sensor = millis();
        ecu.request_sensors();
        ecu.keep_alive();
    }

    // Draw current page
    spr.fillSprite(BG_COLOR);
    switch (current_page) {
        case PAGE_MAIN:      draw_page_main(); break;
        case PAGE_GAUGES:    draw_page_gauges(); break;
        case PAGE_INJECTORS: draw_page_injectors(); break;
        case PAGE_DTCS:      draw_page_dtcs(); break;
    }
    spr.pushSprite(0, 0);

    // Check for touch to cycle pages
    if (tft.getTouchRaw()) {
        current_page = (Page)((current_page + 1) % 4);
        delay(300); // debounce
    }

    delay(50);
}

// ─── SPLASH SCREEN ───
void draw_splash() {
    tft.fillScreen(BG_COLOR);
    tft.setTextSize(2);
    tft.setTextColor(RPM_COLOR);
    tft.setCursor(40, 60);
    tft.println(F("SDS Reader v2.0"));
    tft.setTextColor(TEXT_COLOR);
    tft.setTextSize(1);
    tft.setCursor(50, 100);
    tft.println(F("2004 GSX-R1000"));
    tft.setCursor(30, 130);
    tft.println(F("Initializing K-Line..."));
    tft.drawRect(40, 160, 240, 20, TFT_BLUE);
    for (int i = 0; i < 240; i += 10) {
        tft.fillRect(40 + i, 161, 8, 18, TFT_BLUE);
        delay(50);
    }
}

// ─── PAGE 1: MAIN DASHBOARD ───
void draw_page_main() {
    // RPM gauge (analog needle)
    spr.fillCircle(100, 150, 80, TFT_DARKGREY);
    spr.fillCircle(100, 150, 72, BG_COLOR);

    float rpm_angle = map(constrain(ecu.rpm, 0, 14000), 0, 14000, 135, 405);
    float rad = rpm_angle * PI / 180.0;
    int16_t nx = 100 + 65 * cos(rad);
    int16_t ny = 150 + 65 * sin(rad);
    spr.drawLine(100, 150, nx, ny, RPM_COLOR);

    // RPM value
    spr.setTextDatum(MC_DATUM);
    spr.setTextSize(3);
    spr.setTextColor(RPM_COLOR);
    spr.drawNumber(ecu.rpm, 100, 120);

    // RPM labels
    spr.setTextSize(1);
    spr.setTextColor(TEXT_COLOR);
    spr.drawString("0", 50, 190);
    spr.drawString("7k", 85, 30);
    spr.drawString("14k", 145, 190);

    // Digital speed
    spr.setTextSize(5);
    spr.setTextColor(TFT_GREEN);
    spr.drawNumber(ecu.speed, 260, 60);

    spr.setTextSize(1);
    spr.setTextColor(TEXT_COLOR);
    spr.drawString("km/h", 270, 100);

    // Gear
    spr.setTextSize(4);
    spr.setTextColor(TFT_YELLOW);
    if (ecu.gear == 0)
        spr.drawString("N", 260, 130);
    else
        spr.drawNumber(ecu.gear, 260, 130);

    // Side status bar
    spr.setTextSize(1);
    spr.setTextColor(TEXT_COLOR);
    spr.drawString("ECT", 10, 10);
    spr.setTextColor(BAR_COLOR);
    spr.drawNumber(ecu.ect, 50, 10);
    spr.drawString("C", 80, 10);

    spr.setTextColor(TEXT_COLOR);
    spr.drawString("BAT", 140, 10);
    spr.setTextColor(BAR_COLOR);
    spr.drawFloat(ecu.battery_v, 1, 180, 10);

    // Warning indicators
    if (ecu.ect > 100) {
        spr.setTextColor(ALERT_COLOR);
        spr.drawString("!HOT", 240, 180);
    }
    if (ecu.battery_v < 11.0) {
        spr.setTextColor(ALERT_COLOR);
        spr.drawString("!BAT", 240, 200);
    }
    if (ecu.clutch) {
        spr.setTextColor(TFT_ORANGE);
        spr.drawString("CLUTCH", 240, 10);
    }

    // TPS bar
    spr.fillRect(10, 210, map(ecu.tps, 0, 100, 0, 300), 8, TFT_BLUE);
    spr.drawRect(10, 210, 300, 8, TFT_WHITE);
    spr.setTextColor(TEXT_COLOR);
    spr.drawString("TPS " + String(ecu.tps) + "%", 180, 218);

    // Page indicator
    spr.setTextColor(TFT_DARKGREY);
    spr.drawString("< TAP TO SWITCH >", 100, 228);
}

// ─── PAGE 2: ANALOG GAUGES ───
void draw_page_gauges() {
    // Coolant temp gauge
    spr.setTextSize(1);
    spr.setTextColor(TFT_CYAN);
    spr.drawString("COOLANT", 20, 10);
    int ect_val = map(constrain(ecu.ect, 0, 120), 0, 120, 0, 200);
    spr.fillRect(20, 30, ect_val, 20, TFT_RED);
    spr.drawRect(20, 30, 200, 20, TFT_WHITE);
    spr.setTextColor(TEXT_COLOR);
    spr.drawNumber(ecu.ect, 230, 35);
    spr.drawString("C", 250, 35);

    // IAT gauge
    spr.setTextColor(TFT_CYAN);
    spr.drawString("INTAKE AIR", 20, 60);
    int iat_val = map(constrain(ecu.iat, 0, 80), 0, 80, 0, 200);
    spr.fillRect(20, 80, iat_val, 20, TFT_ORANGE);
    spr.drawRect(20, 80, 200, 20, TFT_WHITE);
    spr.setTextColor(TEXT_COLOR);
    spr.drawNumber(ecu.iat, 230, 85);
    spr.drawString("C", 250, 85);

    // MAP gauge
    spr.setTextColor(TFT_CYAN);
    spr.drawString("MAP", 20, 110);
    int map_val = map(constrain(ecu.map_kpa, 0, 105), 0, 105, 0, 200);
    spr.fillRect(20, 130, map_val, 20, TFT_PURPLE);
    spr.drawRect(20, 130, 200, 20, TFT_WHITE);
    spr.setTextColor(TEXT_COLOR);
    spr.drawNumber(ecu.map_kpa, 230, 135);
    spr.drawString("kPa", 250, 135);

    // Battery gauge
    spr.setTextColor(TFT_CYAN);
    spr.drawString("BATTERY", 20, 160);
    int batt_val = map(constrain(ecu.battery_v * 10, 100, 150), 100, 150, 0, 200);
    uint16_t batt_color = ecu.battery_v > 12.0 ? TFT_GREEN : TFT_ORANGE;
    spr.fillRect(20, 180, batt_val, 20, batt_color);
    spr.drawRect(20, 180, 200, 20, TFT_WHITE);
    spr.setTextColor(TEXT_COLOR);
    spr.drawFloat(ecu.battery_v, 1, 230, 185);

    // Ignition angle
    spr.setTextColor(TFT_CYAN);
    spr.drawString("IGN ANGLE", 20, 210);
    spr.setTextColor(TFT_YELLOW);
    spr.drawNumber(ecu.ignition_angle, 120, 210);
    spr.drawString("deg BTDC", 145, 210);

    // Page indicator
    spr.setTextColor(TFT_DARKGREY);
    spr.drawString("PAGE 2/4", 260, 228);
}

// ─── PAGE 3: INJECTOR MONITOR ───
void draw_page_injectors() {
    spr.setTextSize(2);
    spr.setTextColor(TFT_CYAN);
    spr.drawString("INJECTOR PULSE", 20, 10);

    uint16_t colors[4] = { TFT_RED, TFT_GREEN, TFT_BLUE, TFT_YELLOW };
    for (int i = 0; i < 4; i++) {
        int bar_len = map(constrain(ecu.injector_pw[i], 0, 5000), 0, 5000, 0, 280);
        spr.fillRect(20, 50 + i * 45, bar_len, 30, colors[i]);
        spr.drawRect(20, 50 + i * 45, 280, 30, TFT_WHITE);

        spr.setTextSize(1);
        spr.setTextColor(TEXT_COLOR);
        spr.drawString("INJ " + String(i + 1), 25, 58 + i * 45);

        spr.setTextColor(colors[i]);
        spr.drawNumber(ecu.injector_pw[i], 200, 58 + i * 45);
        spr.drawString("us", 240, 58 + i * 45);
    }

    // Page indicator
    spr.setTextColor(TFT_DARKGREY);
    spr.drawString("PAGE 3/4", 260, 228);
}

// ─── PAGE 4: DTC READER ───
void draw_page_dtcs() {
    spr.setTextSize(2);
    spr.setTextColor(TFT_CYAN);
    spr.drawString("DIAGNOSTIC CODES", 20, 10);

    spr.setTextSize(1);
    spr.setTextColor(TEXT_COLOR);
    spr.drawString("Tap to read DTCs", 20, 50);

    if (ecu.read_dtcs()) {
        uint8_t *r = ecu.get_response();
        uint8_t start = ecu.get_response_data_start();
        uint8_t count = r[start + 1];

        spr.setTextColor(count > 0 ? TFT_RED : TFT_GREEN);
        spr.drawNumber(count, 20, 80);
        spr.drawString(" code(s) stored", 50, 80);
    } else {
        spr.setTextColor(TFT_RED);
        spr.drawString("Read failed", 20, 80);
    }

    // Status
    spr.setTextColor(TFT_DARKGREY);
    spr.drawString("PAGE 4/4", 260, 228);
}

void draw_status(const char *msg, uint16_t color) {
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(color);
    tft.setTextSize(1);
    tft.setCursor(20, 100);
    tft.println(msg);
}
