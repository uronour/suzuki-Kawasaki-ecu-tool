/*
 * ECU Simulator — Bench test your interface without the bike
 *
 * This simulates a Suzuki SDS ECU on the K-Line.
 * Connect the L9637D as usual, but instead of the bike,
 * connect K-Line, +12V, and GND between two Arduinos.
 *
 * This Arduino acts as the ECU, responding to diagnostic requests.
 * Use it to test your SDS reader code before touching the bike.
 */

#include <Arduino.h>

#define ECU_BAUD 10400
#define CONSOLE_BAUD 115200
#define OUR_ADDR 0x12     // ECU address
#define TESTER_ADDR 0xF1  // Tester address

#if defined(ARDUINO_ARCH_ESP32)
HardwareSerial kline(2);
#else
#define kline Serial1
#endif

#define LED_PIN 13

uint8_t rx_buf[64];
uint8_t rx_idx = 0;
unsigned long last_byte = 0;

// Simulated sensor data
uint8_t response_data[] = {
    0x00, // placeholder for SID response
    0x00, // data length
    // 17 bytes of sensor data starting from index 16-52
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x64, // [16] speed = 100 = 200 km/h
    0xC8, // [17] RPM high = 200
    0x00, // [18] RPM low = 0  -> 2000 RPM
    0x8C, // [19] TPS raw -> ~50%
    0x4B, // [20] MAP = 75 -> ~40 kPa
    0x78, // [21] ECT = 120 -> 45 C
    0x50, // [22] IAT = 80 -> 20 C
    0x00, // [23]
    0x7E, // [24] Battery = 126 -> ~12.5V
    0x00, // [25]
    0x03, // [26] Gear = 3rd
    0x4B, // [27] Baro
    0x00, 0x00, // [28-29]
    0x00, // [30]
    0x03, 0xE8, // [31-32] Injector 1 = 1000us
    0x03, 0xE8, // [33-34] Injector 2 = 1000us
    0x03, 0xE8, // [35-36] Injector 3 = 1000us
    0x03, 0xE8, // [37-38] Injector 4 = 1000us
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x2C, // [46] STPS = 44 -> ~17%
    0x1E, // [47] Ign angle = 30 deg
    0x00, 0x00, 0x00,
    0x01, // [51] PAIR
    0x01, // [52] Clutch engaged
    0x01, // [53] Neutral
};

void setup() {
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);
    Serial.begin(CONSOLE_BAUD);
    kline.begin(ECU_BAUD);
    Serial.println(F("ECU Simulator Ready"));
    Serial.println(F("Waiting for tester connection..."));
}

void loop() {
    while (kline.available()) {
        digitalWrite(LED_PIN, HIGH);
        uint8_t b = kline.read();
        unsigned long now = millis();

        if (now - last_byte > 20 && rx_idx > 0) {
            process_frame();
            rx_idx = 0;
        }

        if (rx_idx < sizeof(rx_buf)) {
            rx_buf[rx_idx++] = b;
        }
        last_byte = millis();
        digitalWrite(LED_PIN, LOW);
    }

    if (rx_idx > 0 && millis() - last_byte > 20) {
        process_frame();
        rx_idx = 0;
    }
}

uint8_t calc_cs(uint8_t *buf, uint8_t len) {
    uint8_t sum = 0;
    for (uint8_t i = 0; i < len; i++) sum += buf[i];
    return sum;
}

void send_response(uint8_t *data, uint8_t len) {
    uint8_t cs = calc_cs(data, len);
    for (uint8_t i = 0; i < len; i++) {
        kline.write(data[i]);
    }
    kline.write(cs);
    kline.flush();
}

void process_frame() {
    Serial.print(F("Got "));
    Serial.print(rx_idx);
    Serial.print(F(" bytes: "));
    for (uint8_t i = 0; i < rx_idx; i++) {
        Serial.print(rx_buf[i], HEX);
        Serial.print(F(" "));
    }
    Serial.println();

    // Validate checksum
    if (rx_idx < 3) return;
    uint8_t cs = calc_cs(rx_buf, rx_idx - 1);
    if (cs != rx_buf[rx_idx - 1]) {
        Serial.println(F("Bad CS, ignoring"));
        return;
    }

    // Check addressed to us
    uint8_t sid = rx_buf[3];

    switch (sid) {
        case 0x81: { // startCommunication
            uint8_t resp[] = {0x80, 0x12, 0xF1, 0xC1, 0x00, 0x94, 0x00};
            send_response(resp, sizeof(resp));
            Serial.println(F("-> startCommunication ACK"));
            break;
        }
        case 0x82: { // stopCommunication
            uint8_t resp[] = {0x80, 0x12, 0xF1, 0xC2};
            send_response(resp, sizeof(resp));
            Serial.println(F("-> stopCommunication ACK"));
            break;
        }
        case 0x21: { // readDataByLocalIdentifier
            uint8_t id = rx_buf[4];
            if (id == 0x08) {
                uint8_t resp[64];
                uint8_t idx = 0;
                resp[idx++] = 0x80 | (sizeof(response_data));
                resp[idx++] = OUR_ADDR;
                resp[idx++] = TESTER_ADDR;
                resp[idx++] = 0x61; // response SID
                for (uint8_t i = 0; i < sizeof(response_data); i++) {
                    resp[idx++] = response_data[i];
                }
                send_response(resp, idx);
                Serial.println(F("-> sensor data response"));
            } else {
                uint8_t resp[] = {0x83, 0x12, 0xF1, 0x7F, 0x21, 0x11};
                send_response(resp, sizeof(resp));
                Serial.println(F("-> NACK: serviceNotSupported"));
            }
            break;
        }
        case 0x3E: { // testerPresent
            uint8_t resp[] = {0x80, 0x12, 0xF1, 0x7E, 0x01};
            send_response(resp, sizeof(resp));
            break;
        }
        case 0x13: { // readDiagnosticTroubleCodes
            uint8_t resp[] = {0x81, 0x12, 0xF1, 0x53, 0x00};
            send_response(resp, sizeof(resp));
            Serial.println(F("-> DTC response (no codes)"));
            break;
        }
        default:
            Serial.print(F("Unknown SID: 0x"));
            Serial.println(sid, HEX);
            break;
    }
}
