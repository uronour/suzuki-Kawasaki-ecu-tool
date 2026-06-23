/*
 * SDS Standalone Reader for 2004 Suzuki GSX-R1000
 *
 * This version uses the built-in SDSProtocol library (no external deps).
 * Connect via USB serial at 115200 baud for the command interface.
 *
 * Hardware: Arduino Nano + L9637D + 4N35 optocoupler
 * See hardware/wiring.md for connection details.
 */

#include "SDSProtocol.h"

#if defined(ARDUINO_ARCH_ESP32)
HardwareSerial kline(2);
#define TX_PIN 17
#define DEALER_PIN 4
#else
#define kline Serial
#define TX_PIN 1
#define DEALER_PIN 4
#endif

SDSProtocol ecu(&kline, TX_PIN, DEALER_PIN);

unsigned long last_sensor_request = 0;
const unsigned long SENSOR_INTERVAL = 250;

void setup() {
    Serial.begin(115200);
    while (!Serial) { ; }

    Serial.println(F("========================================"));
    Serial.println(F("SDS Standalone Reader v1.0"));
    Serial.println(F("2004 Suzuki GSX-R1000 K-Line Interface"));
    Serial.println(F("========================================"));

    ecu.enable_debug(&Serial);

    Serial.println(F("\nCommands:"));
    Serial.println(F("  i = initialize K-Line"));
    Serial.println(F("  s = request sensor data (once)"));
    Serial.println(F("  a = auto-poll sensors (every 250ms)"));
    Serial.println(F("  d = toggle dealer mode"));
    Serial.println(F("  t = read DTCs"));
    Serial.println(F("  c = clear DTCs"));
    Serial.println(F("  x = print raw last response"));
    Serial.println(F("  q = disconnect"));
    Serial.println(F("  ? = help"));
}

void loop() {
    if (Serial.available() > 0) {
        char c = Serial.read();
        switch (c) {
            case 'i': {
                Serial.println(F("\n--- INIT ---"));
                if (ecu.begin()) {
                    Serial.println(F("Connected to ECU!"));
                } else {
                    Serial.println(F("Connection failed. Check wiring."));
                }
                break;
            }
            case 's': {
                Serial.println(F("\n--- SENSORS ---"));
                if (ecu.request_sensors()) {
                    print_sensors();
                } else {
                    Serial.println(F("Request failed"));
                }
                break;
            }
            case 'a': {
                static bool auto_mode = false;
                auto_mode = !auto_mode;
                Serial.print(F("Auto-poll: "));
                Serial.println(auto_mode ? F("ON") : F("OFF"));
                while (auto_mode) {
                    if (ecu.is_connected() && ecu.request_sensors()) {
                        print_sensors_csv();
                    }
                    if (Serial.available()) {
                        char ch = Serial.read();
                        if (ch == 'a' || ch == 'q') auto_mode = false;
                    }
                    delay(SENSOR_INTERVAL);
                }
                break;
            }
            case 'd': {
                bool state = !ecu.get_dealer_mode();
                ecu.set_dealer_mode(state);
                break;
            }
            case 't': {
                Serial.println(F("\n--- DTCs ---"));
                if (ecu.read_dtcs()) {
                    uint8_t *r = ecu.get_response();
                    uint8_t start = ecu.get_response_data_start();
                    uint8_t len = ecu.get_response_len();
                    uint8_t count = r[start + 1];
                    Serial.print(count);
                    Serial.println(F(" DTC(s) found"));
                    for (uint8_t i = 0; i < count; i++) {
                        uint8_t idx = start + 2 + i * 2;
                        if (idx + 1 < len) {
                            Serial.print(F("  DTC: "));
                            Serial.print(r[idx], HEX);
                            Serial.println(r[idx + 1], HEX);
                        }
                    }
                } else {
                    Serial.println(F("Failed to read DTCs"));
                }
                break;
            }
            case 'c': {
                Serial.println(F("Clearing DTCs..."));
                Serial.println(ecu.clear_dtcs() ? F("Done") : F("Failed"));
                break;
            }
            case 'x': {
                Serial.println(F("\n--- RAW ---"));
                uint8_t *r = ecu.get_response();
                uint8_t len = ecu.get_response_len();
                for (uint8_t i = 0; i < len; i++) {
                    Serial.print(F("0x"));
                    if (r[i] < 0x10) Serial.print(F("0"));
                    Serial.print(r[i], HEX);
                    Serial.print(F(" "));
                }
                Serial.println();
                break;
            }
            case 'q': {
                Serial.println(F("Disconnecting..."));
                ecu.end();
                break;
            }
            case '?': {
                Serial.println(F("i=init, s=sensors, a=auto, d=dealer, t=DTC, c=clear, x=raw, q=quit"));
                break;
            }
        }
    }

    static unsigned long last_keepalive = 0;
    if (ecu.is_connected() && millis() - last_keepalive > 1000) {
        ecu.keep_alive();
        last_keepalive = millis();
    }
}

void print_sensors() {
    Serial.print(F("RPM: ")); Serial.println(ecu.rpm);
    Serial.print(F("Speed: ")); Serial.print(ecu.speed); Serial.println(F(" km/h"));
    Serial.print(F("Gear: ")); Serial.println(ecu.gear);
    Serial.print(F("TPS: ")); Serial.print(ecu.tps); Serial.println(F("%"));
    Serial.print(F("STPS: ")); Serial.print(ecu.stps); Serial.println(F("%"));
    Serial.print(F("ECT: ")); Serial.print(ecu.ect); Serial.println(F(" C"));
    Serial.print(F("IAT: ")); Serial.print(ecu.iat); Serial.println(F(" C"));
    Serial.print(F("MAP: ")); Serial.print(ecu.map_kpa); Serial.println(F(" kPa"));
    Serial.print(F("Baro: ")); Serial.print(ecu.baro_kpa); Serial.println(F(" kPa"));
    Serial.print(F("Battery: ")); Serial.print(ecu.battery_v, 1); Serial.println(F(" V"));
    Serial.print(F("Ignition: ")); Serial.print(ecu.ignition_angle); Serial.println(F(" deg"));
    Serial.print(F("Clutch: ")); Serial.println(ecu.clutch ? F("IN") : F("OUT"));
    Serial.print(F("Neutral: ")); Serial.println(ecu.neutral ? F("N") : F("IN GEAR"));
    for (int i = 0; i < 4; i++) {
        Serial.print(F("Injector ")); Serial.print(i + 1);
        Serial.print(F(": ")); Serial.print(ecu.injector_pw[i]);
        Serial.println(F(" us"));
    }
    Serial.println(F("---"));
}

void print_sensors_csv() {
    Serial.print(millis()); Serial.print(F(","));
    Serial.print(ecu.rpm); Serial.print(F(","));
    Serial.print(ecu.speed); Serial.print(F(","));
    Serial.print(ecu.tps); Serial.print(F(","));
    Serial.print(ecu.ect); Serial.print(F(","));
    Serial.print(ecu.iat); Serial.print(F(","));
    Serial.print(ecu.map_kpa); Serial.print(F(","));
    Serial.print(ecu.battery_v, 1); Serial.print(F(","));
    Serial.print(ecu.gear); Serial.print(F(","));
    Serial.print(ecu.ignition_angle); Serial.print(F(","));
    for (int i = 0; i < 4; i++) {
        Serial.print(ecu.injector_pw[i]);
        Serial.print(i < 3 ? F(",") : F("\n"));
    }
}
