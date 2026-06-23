/*
 * K-Line Sniffer — Raw bus monitor for protocol debugging
 *
 * Use a second UART or logic analyzer on the TX pin.
 * This passively monitors the K-Line and prints all traffic.
 * Useful for reverse-engineering or verifying communication.
 */

#include <Arduino.h>

#define SNAPPER_BAUD 10400
#define CONSOLE_BAUD 115200

// For Arduino Nano: use SoftwareSerial or AltSoftSerial
// For Arduino Mega: use Serial1 (pins 19 RX, 18 TX)
// For ESP32: use Serial2 (pins 16 RX, 17 TX)

#if defined(ARDUINO_ARCH_ESP32)
HardwareSerial kline(2);
#else
#define kline Serial1
#endif

#define LED_PIN 13

uint8_t buf[512];
uint8_t idx = 0;
unsigned long last_byte = 0;
const unsigned long FRAME_GAP = 20;

void setup() {
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);
    Serial.begin(CONSOLE_BAUD);
    kline.begin(SNAPPER_BAUD);
    Serial.println(F("K-Line Sniffer Ready"));
    Serial.println(F("Listening on Serial1 @ 10400 baud"));
    Serial.println(F("===================================="));
    idx = 0;
    last_byte = millis();
}

void loop() {
    while (kline.available()) {
        digitalWrite(LED_PIN, HIGH);
        uint8_t b = kline.read();
        unsigned long now = millis();

        if (now - last_byte > FRAME_GAP && idx > 0) {
            // Frame gap detected — print previous frame
            print_frame();
            idx = 0;
        }

        if (idx < sizeof(buf)) {
            buf[idx++] = b;
        }
        last_byte = millis();
        digitalWrite(LED_PIN, LOW);
    }

    // Flush any remaining frame
    if (idx > 0 && millis() - last_byte > FRAME_GAP) {
        print_frame();
        idx = 0;
    }
}

void print_frame() {
    if (idx < 3) return;

    // Determine direction based on first byte's address field
    if (buf[1] == 0xF1) {
        Serial.print(F("[ECU->SDT] "));
    } else if (buf[1] == 0x12) {
        Serial.print(F("[SDT->ECU] "));
    } else if (buf[1] == 0x11) {
        Serial.print(F("[ECU->SDT] "));
    } else {
        Serial.print(F("[UNKNOWN] "));
    }

    Serial.print(idx);
    Serial.print(F(" bytes: "));

    uint8_t cs = 0;
    for (uint8_t i = 0; i < idx; i++) {
        if (buf[i] < 0x10) Serial.print(F("0"));
        Serial.print(buf[i], HEX);
        Serial.print(F(" "));
        if (i < idx - 1) cs += buf[i];
    }
    cs &= 0xFF;

    Serial.print(F(" | CS=0x"));
    if (cs < 0x10) Serial.print(F("0"));
    Serial.print(cs, HEX);

    if (cs == buf[idx - 1]) {
        Serial.println(F(" [OK]"));
    } else {
        Serial.println(F(" [BAD]"));
    }
}
