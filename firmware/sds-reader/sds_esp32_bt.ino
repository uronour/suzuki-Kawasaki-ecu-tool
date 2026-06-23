/*
 * sds_esp32_bt.ino — ESP32 Bluetooth SPP Telemetry
 *
 * Streams all SDS sensor data via Bluetooth Classic SPP.
 * Pair with "SDS-Reader" (PIN: 1234), then connect with:
 *   - Python dashboard: sds_dashboard.py (BT mode)
 *   - Serial BT terminal app on phone
 *
 * Data format: CSV, 10Hz
 *   RPM,Speed,Gear,TPS,ECT,IAT,MAP,Battery,IgnAng,Inj1,Inj2,Inj3,Inj4,Clutch,Neutral
 *
 * Hardware: ESP32 + L9637D + 4N35
 * See hardware/esp32_upgrade.md for pinout
 */

#include "SDSProtocol.h"
#include "BluetoothSerial.h"

BluetoothSerial SerialBT;

#define BT_NAME "SDS-Reader"
#define KLINE_RX 16
#define KLINE_TX 17
#define DEALER_PIN 4
#define BT_BAUD 115200

// Use UART2 for K-Line
HardwareSerial kline(2);
SDSProtocol ecu(&kline, KLINE_TX, DEALER_PIN);

unsigned long last_sensor_ms = 0;
const unsigned long SENSOR_INTERVAL_MS = 100;
unsigned long last_status_ms = 0;
bool bt_connected = false;

void setup() {
    Serial.begin(115200);
    SerialBT.begin(BT_NAME);
    Serial.println(F("SDS ESP32 Bluetooth Reader"));
    Serial.print(F("BT name: ")); Serial.println(BT_NAME);
    Serial.println(F("Pair with PIN: 1234"));

    pinMode(DEALER_PIN, OUTPUT);
    digitalWrite(DEALER_PIN, LOW);

    pinMode(2, OUTPUT); // built-in LED for status
    digitalWrite(2, LOW);
}

void loop() {
    // Handle BT connection events
    if (SerialBT.hasClient() && !bt_connected) {
        bt_connected = true;
        digitalWrite(2, HIGH);
        SerialBT.println(F("SDS-Reader connected"));
        SerialBT.println(F("CSV: RPM,Speed,Gear,TPS,ECT,IAT,MAP,BattV,IgnAng,Inj1,Inj2,Inj3,Inj4,Clutch,Neutral"));
        Serial.println(F("BT client connected"));
    }
    if (!SerialBT.hasClient() && bt_connected) {
        bt_connected = false;
        digitalWrite(2, LOW);
        Serial.println(F("BT client disconnected"));
    }

    // Handle commands from BT
    if (bt_connected && SerialBT.available()) {
        handle_command(SerialBT.read());
    }
    if (Serial.available()) {
        handle_command(Serial.read());
    }

    // Auto-init on first loop
    static bool inited = false;
    if (!inited) {
        static unsigned long init_start = 0;
        if (init_start == 0) init_start = millis();
        if (millis() - init_start > 2000) { // wait 2s for BT to init
            Serial.println(F("Initializing K-Line..."));
            if (ecu.begin()) {
                inited = true;
                Serial.println(F("ECU connected!"));
                SerialBT.println(F("ECU connected"));
                ecu.set_dealer_mode(true);
            } else {
                Serial.println(F("Init failed, retrying in 5s..."));
                init_start = millis(); // retry
            }
        }
        return;
    }

    // Periodic sensor polling
    if (ecu.is_connected() && millis() - last_sensor_ms > SENSOR_INTERVAL_MS) {
        last_sensor_ms = millis();
        if (ecu.request_sensors()) {
            send_csv_bt();
            send_csv_serial();
        }
    }

    // Keep-alive
    static unsigned long last_ka = 0;
    if (ecu.is_connected() && millis() - last_ka > 500) {
        ecu.keep_alive();
        last_ka = millis();
    }
}

void handle_command(char c) {
    switch (c) {
        case 'i':
            if (!ecu.is_connected()) {
                SerialBT.println(F("Init..."));
                ecu.begin();
            }
            break;
        case 'd':
            ecu.set_dealer_mode(!ecu.get_dealer_mode());
            break;
        case 's':
            if (ecu.request_sensors()) {
                send_csv_bt();
            }
            break;
        case 't':
            ecu.read_dtcs();
            break;
        case 'q':
            ecu.end();
            break;
    }
}

void send_csv_bt() {
    if (!bt_connected) return;
    SerialBT.print(ecu.rpm); SerialBT.print(',');
    SerialBT.print(ecu.speed); SerialBT.print(',');
    SerialBT.print(ecu.gear); SerialBT.print(',');
    SerialBT.print(ecu.tps); SerialBT.print(',');
    SerialBT.print(ecu.ect); SerialBT.print(',');
    SerialBT.print(ecu.iat); SerialBT.print(',');
    SerialBT.print(ecu.map_kpa); SerialBT.print(',');
    SerialBT.print(ecu.battery_v, 1); SerialBT.print(',');
    SerialBT.print(ecu.ignition_angle); SerialBT.print(',');
    for (int i = 0; i < 4; i++) {
        SerialBT.print(ecu.injector_pw[i]);
        SerialBT.print(i < 3 ? ',' : '\n');
    }
}

void send_csv_serial() {
    Serial.print(millis()); Serial.print(',');
    Serial.print(ecu.rpm); Serial.print(',');
    Serial.print(ecu.speed); Serial.print(',');
    Serial.print(ecu.gear); Serial.print(',');
    Serial.print(ecu.tps); Serial.print(',');
    Serial.print(ecu.ect); Serial.print(',');
    Serial.print(ecu.iat); Serial.print(',');
    Serial.print(ecu.map_kpa); Serial.print(',');
    Serial.print(ecu.battery_v, 1); Serial.print(',');
    Serial.print(ecu.ignition_angle); Serial.print(',');
    for (int i = 0; i < 4; i++) {
        Serial.print(ecu.injector_pw[i]);
        Serial.print(i < 3 ? ',' : '\n');
    }
}
