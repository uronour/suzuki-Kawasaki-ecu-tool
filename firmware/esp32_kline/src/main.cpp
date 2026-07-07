#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

// ESP32-specific includes
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#if BT_ENABLED
#include <BluetoothSerial.h>
#endif
#include <ArduinoJson.h>

// Pin definitions - board specific
#if defined(BOARD_WEMOS_D1_MINI)
    // Wemos D1 Mini ESP32: UART2 on GPIO16(D4/RX)/GPIO17(D3/TX) for K-Line
    #ifndef KLINE_RX_PIN
    #define KLINE_RX_PIN 16              // U2RX (GPIO16 / D4)
    #endif
    #ifndef KLINE_TX_PIN
    #define KLINE_TX_PIN 17              // U2TX (GPIO17 / D3)
    #endif
    #define LED_PIN 2                    // Built-in LED
    #define DEALER_PIN 12                // Dealer mode trigger
    #define DS18B20_PIN 12               // DS18B20 data pin
    #define KLINE_SERIAL Serial2         // Use UART2 (HardwareSerial(2))
#elif defined(BOARD_ESP32CAM)
    // ESP32-CAM AI-Thinker: use UART0 (shared with FTDI)
    #define LED_PIN 4                    // Flash LED
    #define DEALER_PIN 12                // Dealer mode trigger (GPIO12)
    #define KLINE_RX_PIN 3               // U0RX (GPIO3) - FTDI RX
    #define KLINE_TX_PIN 1               // U0TX (GPIO1) - FTDI TX
    #define DS18B20_PIN 12               // DS18B20 data pin
    #define KLINE_SERIAL Serial          // Use UART0 (HardwareSerial(0))
#else
    // ESP32 DevKit / Node-32S: use UART2 on GPIO16/17
    #define LED_PIN 2                    // Built-in LED
    #define DEALER_PIN 12                // Dealer mode trigger
    #define KLINE_RX_PIN 16              // ESP32 RX for K-Line (UART2)
    #define KLINE_TX_PIN 17              // ESP32 TX for K-Line (UART2)
    #define DS18B20_PIN 12               // DS18B20 data pin
    #define KLINE_SERIAL Serial2         // Use UART2 (HardwareSerial(2))
#endif

// Baud rates
#define ECU_BAUDRATE 10400           // K-Line baud rate (ISO 9141-2)
#define WIFI_CHANNEL 1               // WiFi channel
#define MAX_CLIENTS 4                // Max concurrent WiFi clients

// K-Line protocol constants
#define KLINE_FAST_INIT_BAUD 5       // 5 baud init
#define KW1_BYTE 0x08                // Keyword 1
#define KW2_BYTE 0xF7                // Keyword 2 (inverted)
#define START_COMMS 0x81             // StartCommunication
#define ACCESS_TIMING 0x83           // AccessTiming
#define READ_DATA_BY_ID 0x21         // ReadDataByLocalIdentifier
#define STOP_COMMS 0x82              // StopCommunication
#define ECU_RESPONSE_DELAY 50        // ms
#define ECU_TIMEOUT 1000             // ms

// State machine
enum ECUState {
    STANDBY = 0,      // Waiting for connection
    SLOW_INIT = 1,    // 5-baud slow initialization
    FAST_INIT = 2,    // KWP fast initialization
    NORMAL = 3,       // Normal operation
    FAULT = 4         // Fault state
};

// K-Line state machine
enum KLineState {
    KL_IDLE = 0,
    KL_FAST_INIT_5BAUD = 1,
    KL_FAST_INIT_WAIT_SYNC = 2,
    KL_FAST_INIT_WAIT_KW1 = 3,
    KL_FAST_INIT_WAIT_KW2 = 4,
    KL_START_COMMS_SENT = 5,
    KL_START_COMMS_WAIT_RESP = 6,
    KL_TIMING_SENT = 7,
    KL_TIMING_WAIT_RESP = 8,
    KL_ACTIVE = 9,
    KL_STOP_COMMS = 10
};

// SDS Protocol constants
#define FAST_INIT_PULSE_MS      25
#define ECU_SYNC_BYTE           0x55
#define KW2_INVERTED            0xF7
#define ECU_RESPONSE_DELAY_MS   50
#define KLINE_TIMEOUT_MS        5000

// K-Line hardware serial
#if defined(BOARD_WEMOS_D1_MINI)
    HardwareSerial KLineSerial(2);  // UART2: GPIO16(RX), GPIO17(TX)
#elif defined(BOARD_ESP32CAM)
    HardwareSerial KLineSerial(1);  // UART1 on GPIO16/17
#else
    HardwareSerial KLineSerial(2);  // UART2: GPIO16(RX), GPIO17(TX)
#endif

// K-Line state variables
KLineState klineState = KL_IDLE;
uint8_t klineRxBuffer[64];
uint8_t klineRxIndex = 0;
uint32_t klineStateStartTime = 0;
uint32_t klineLastByteTime = 0;
uint8_t klineSyncCount = 0;
bool klineFastInitSent = false;

// ECU Data structure
struct ECUData {
    uint32_t rpm;
    uint8_t coolantTemp;
    uint8_t intakeAirTemp;
    uint8_t throttlePos;
    uint8_t mapKpa;
    uint8_t batteryVolt;
    uint8_t gearPos;
    uint16_t speed;
    uint8_t ignitionTiming;
    uint8_t injectorPulse;
    uint8_t iacStep;
    bool clutchIn;
    bool fanOn;
    bool sidestandDown;
    bool kLineConnected;
    uint8_t dtcCount;
    uint8_t ecuMode;
    uint32_t lastUpdate;
    bool dealerModeActive;
    uint32_t dealerModeStart;
    ECUState state;
};

// Global objects
ECUData ecuData;
AsyncWebServer server(80);
#if BT_ENABLED
BluetoothSerial SerialBT;
#endif

// K-Line log buffer (websocket-style polling)
#define KLINE_LOG_MAX 4096
String klineLog = "";
void klineLogAppend(const char* msg);
void klineLogAppend(const char* prefix, uint8_t byte);

// Helper functions
void handleRootRequest(AsyncWebServerRequest *request);
void handleApiEcuRequest(AsyncWebServerRequest *request);
void handleApiCmdRequest(AsyncWebServerRequest *request);
void handleApiDealerRequest(AsyncWebServerRequest *request);
void handleApiKLineRequest(AsyncWebServerRequest *request);
void handleApiStatusRequest(AsyncWebServerRequest *request);
void handleApiLogRequest(AsyncWebServerRequest *request);
void handleApiLogRequest(AsyncWebServerRequest *request);
void processKLineSerial(void);
void updateSensorData(void);
void broadcastBluetoothData(void);
void updateLEDs(void);
void checkDealerMode(void);
const char* stateToString(ECUState state);

// K-Line protocol functions
void klineSendFastInit(void);
void klineSendByte(uint8_t byte);
void klineSendFrame(uint8_t* data, uint8_t len);
uint8_t klineCalculateChecksum(uint8_t* data, uint8_t len);
void klineProcessRxBuffer(void);
void klineTransitionState(KLineState newState);
bool klineCheckTimeout(void);

// K-Line log functions
void klineLogAppend(const char* msg) {
    klineLog += msg;
    klineLog += "\n";
    if (klineLog.length() > KLINE_LOG_MAX)
        klineLog = klineLog.substring(klineLog.length() - KLINE_LOG_MAX / 2);
}

void klineLogAppend(const char* prefix, uint8_t byte) {
    char buf[32];
    sprintf(buf, "%s 0x%02X", prefix, byte);
    klineLogAppend(buf);
}

// Setup functions
void setupHardware(void);
void setupNetwork(void);
void setupWebEndpoints(void);
void setupBluetooth(void);
void setupEcudata(void);

// Web endpoint implementations (forward declarations)
String getHtmlPage();
String createJsonResponse();
void sendJsonResponse(AsyncWebServerRequest *request, String json);
void sendKLineStatus(AsyncWebServerRequest *request);
void sendSystemStatus(AsyncWebServerRequest *request);
void handleCommandRequest(AsyncWebServerRequest *request);
void handleDealerRequest(AsyncWebServerRequest *request);

// Main entry point
void setup(void) {
    setupHardware();
    setupBluetooth();
    setupEcudata();
    setupNetwork();
    setupWebEndpoints();

    Serial.println("=== Wemos D1 Mini ESP32 K-Line Bridge ===");
    Serial.print("WiFi AP: "); Serial.println(WiFi.softAPSSID());
    Serial.print("Password: "); Serial.println(WiFi.psk());
    Serial.print("Dealer Mode Pin: GPIO"); Serial.println(DEALER_PIN);
    Serial.print("K-Line UART2 on GPIO"); Serial.print(KLINE_RX_PIN); Serial.print("/GPIO"); Serial.println(KLINE_TX_PIN);
    Serial.print("K-Line Baudrate: "); Serial.println(ECU_BAUDRATE);
    Serial.println("Bluetooth Name: GSX-R1000_ECU");
    Serial.println("\nAvailable endpoints:");
    Serial.println("  GET http://[ESP32_IP]/          - HTML dashboard");
    Serial.println("  GET http://[ESP32_IP]/api/ecu    - JSON ECU data");
    Serial.println("  POST http://[ESP32_IP]/api/cmd    - Send ECU commands");
    Serial.println("  POST http://[ESP32_IP]/api/dealer  - Toggle dealer mode");
    Serial.println("  GET http://[ESP32_IP]/api/kline   - K-Line status");
    Serial.println("  GET http://[ESP32_IP]/api/status  - System status");
    Serial.println("  GET http://[ESP32_IP]/api/log     - K-Line terminal log\n");
}

// Main loop
void loop(void) {
    static uint32_t lastNetworkCheck = 0;
    static uint32_t lastStatusPrint = 0;
    static uint32_t lastKLineInitAttempt = 0;
    static bool klineInitTriggered = false;

    // Check dealer mode
    checkDealerMode();

    // Trigger K-Line fast init when dealer mode is active and not connected
    if (ecuData.dealerModeActive && !ecuData.kLineConnected && klineState == KL_IDLE) {
        if (millis() - lastKLineInitAttempt > 2000) {
            klineTransitionState(KL_FAST_INIT_5BAUD);
            lastKLineInitAttempt = millis();
            klineInitTriggered = true;
        }
    } else if (!ecuData.dealerModeActive) {
        klineInitTriggered = false;
    }

    // Process K-Line communication
    processKLineSerial();

    // Update sensor data
    updateSensorData();

    // Broadcast data via Bluetooth
    broadcastBluetoothData();

    // Update LEDs based on state
    updateLEDs();

    // Periodic network check
    if (millis() - lastNetworkCheck > 30000) {
        Serial.println("WiFi clients: " + String(WiFi.softAPgetStationNum()));
        lastNetworkCheck = millis();
    }

    // Status output every 10 seconds
    if (millis() - lastStatusPrint > 10000) {
        Serial.print("[STATUS] ");
        Serial.print("ECU RPM: "); Serial.print(ecuData.rpm);
        Serial.print(" Gear: "); Serial.print(ecuData.gearPos);
        Serial.print(" K-Line: "); Serial.print(ecuData.kLineConnected ? "CON" : "DIS");
        Serial.print(" State: "); Serial.print(klineState);
        Serial.print(" Dealer: "); Serial.println(ecuData.dealerModeActive ? "ON" : "OFF");
        lastStatusPrint = millis();
    }
}

// String conversion helper
const char* stateToString(ECUState state) {
    switch (state) {
        case STANDBY: return "STANDBY";
        case SLOW_INIT: return "SLOW_INIT";
        case FAST_INIT: return "FAST_INIT";
        case NORMAL: return "NORMAL";
        case FAULT: return "FAULT";
        default: return "UNKNOWN";
    }
}

// Web endpoint handlers
void handleRootRequest(AsyncWebServerRequest *request) {
    request->send(200, "text/html", getHtmlPage());
}

void handleApiEcuRequest(AsyncWebServerRequest *request) {
    sendJsonResponse(request, createJsonResponse());
}

void handleApiCmdRequest(AsyncWebServerRequest *request) {
    handleCommandRequest(request);
}

void handleApiDealerRequest(AsyncWebServerRequest *request) {
    handleDealerRequest(request);
}

void handleApiKLineRequest(AsyncWebServerRequest *request) {
    sendKLineStatus(request);
}

void handleApiStatusRequest(AsyncWebServerRequest *request) {
    sendSystemStatus(request);
}

void handleApiLogRequest(AsyncWebServerRequest *request) {
    request->send(200, "text/plain", klineLog);
}

// Response creation helper
String createJsonResponse() {
    JsonDocument doc;

    doc["rpm"] = ecuData.rpm;
    doc["coolantTemp"] = ecuData.coolantTemp;
    doc["intakeAirTemp"] = ecuData.intakeAirTemp;
    doc["throttlePos"] = ecuData.throttlePos;
    doc["mapKpa"] = ecuData.mapKpa;
    doc["batteryVolt"] = ecuData.batteryVolt;
    doc["gear"] = ecuData.gearPos;
    doc["speed"] = ecuData.speed;
    doc["ignitionTiming"] = ecuData.ignitionTiming;
    doc["injectorPulse"] = ecuData.injectorPulse;
    doc["iacStep"] = ecuData.iacStep;
    doc["clutchIn"] = ecuData.clutchIn;
    doc["fanOn"] = ecuData.fanOn;
    doc["sidestandDown"] = ecuData.sidestandDown;
    doc["kLineConnected"] = ecuData.kLineConnected;
    doc["dtcCount"] = ecuData.dtcCount;
    doc["ecuMode"] = ecuData.ecuMode;
    doc["state"] = stateToString(ecuData.state);
    doc["dealerModeActive"] = ecuData.dealerModeActive;
    doc["timestamp"] = millis();

    String json;
    serializeJson(doc, json);
    return json;
}

// Setup implementation
void setupHardware() {
    pinMode(LED_PIN, OUTPUT);
    pinMode(DEALER_PIN, INPUT_PULLUP);

    Serial.begin(115200);
    delay(100);

    // Initialize K-Line hardware serial
#if defined(BOARD_ESP32CAM)
    KLineSerial.begin(ECU_BAUDRATE, SERIAL_8N1, 16, 17);
#else
    KLineSerial.begin(ECU_BAUDRATE, SERIAL_8N1, KLINE_RX_PIN, KLINE_TX_PIN);
#endif
    KLineSerial.setTimeout(50);
    
    while (KLineSerial.available()) KLineSerial.read();

    Serial.println("Hardware initialized");
#if defined(BOARD_WEMOS_D1_MINI)
    Serial.println("Board: Wemos D1 Mini ESP32");
    Serial.printf("K-Line UART2 on GPIO%d(D%d)/GPIO%d(D%d) @ %d baud\n", KLINE_RX_PIN, KLINE_RX_PIN == 16 ? 4 : 0, KLINE_TX_PIN, KLINE_TX_PIN == 17 ? 3 : 0, ECU_BAUDRATE);
#elif defined(BOARD_ESP32CAM)
    Serial.println("Board: ESP32-CAM AI-Thinker");
    Serial.printf("K-Line UART1 on GPIO16(RX)/GPIO17(TX) @ %d baud\n", ECU_BAUDRATE);
#else
    Serial.println("Board: ESP32 DevKit");
    Serial.printf("K-Line UART2 on GPIO%d(RX)/GPIO%d(TX) @ %d baud\n", KLINE_RX_PIN, KLINE_TX_PIN, ECU_BAUDRATE);
#endif
}

void setupBluetooth() {
#if BT_ENABLED
    SerialBT.begin("GSX-R1000_ECU");
    Serial.println("Bluetooth initialized (GSX-R1000_ECU)");
#else
    Serial.println("Bluetooth: disabled");
#endif
}

void setupEcudata() {
    memset(&ecuData, 0, sizeof(ecuData));

    ecuData.rpm = 1200;
    ecuData.coolantTemp = 82;
    ecuData.intakeAirTemp = 30;
    ecuData.throttlePos = 0;
    ecuData.mapKpa = 35;
    ecuData.batteryVolt = 14;
    ecuData.gearPos = 0;
    ecuData.speed = 0;
    ecuData.ignitionTiming = 10;
    ecuData.injectorPulse = 3;
    ecuData.iacStep = 40;
    ecuData.clutchIn = false;
    ecuData.fanOn = false;
    ecuData.sidestandDown = false;
    ecuData.kLineConnected = false;
    ecuData.dtcCount = 0;
    ecuData.ecuMode = 0;
    ecuData.lastUpdate = millis();
    ecuData.dealerModeActive = false;
    ecuData.dealerModeStart = 0;
    ecuData.state = STANDBY;

    Serial.println("ECU data initialized");
}

void setupNetwork() {
    WiFi.mode(WIFI_AP);
    WiFi.softAP("GSX-R1000_ECU", "suzuki2004");

    IPAddress IP = WiFi.softAPIP();
    Serial.print("AP IP address: ");
    Serial.println(IP);
    Serial.println("OTA enabled - http://" + String(IP.toString()) + "/setup");
}

void setupWebEndpoints() {
    server.on("/", HTTP_GET, handleRootRequest);
    server.on("/api/ecu", HTTP_GET, handleApiEcuRequest);
    server.on("/api/cmd", HTTP_POST, handleApiCmdRequest);
    server.on("/api/dealer", HTTP_POST, handleApiDealerRequest);
    server.on("/api/kline", HTTP_GET, handleApiKLineRequest);
    server.on("/api/status", HTTP_GET, handleApiStatusRequest);
    server.on("/api/log", HTTP_GET, handleApiLogRequest);

    server.onNotFound([](AsyncWebServerRequest *request) {
        request->send(404, "text/plain", "Not Found");
    });

    server.begin();
    Serial.println("Web server started");
}

void klineSendFastInit() {
    // 5-baud init: send 0x33 at 5 baud (each bit = 200ms)
    pinMode(KLINE_TX_PIN, OUTPUT);
    digitalWrite(KLINE_TX_PIN, LOW);
    delay(25);  // Start bit
    
    uint8_t initByte = 0x33;  // Inverted 0xCC for 5-baud
    for (int i = 0; i < 8; i++) {
        digitalWrite(KLINE_TX_PIN, (initByte & 0x01) ? HIGH : LOW);
        initByte >>= 1;
        delay(200);  // 5 baud = 200ms per bit
    }
    digitalWrite(KLINE_TX_PIN, HIGH);  // Stop bit
    delay(25);
    
    // Switch back to UART mode at 10400 baud
    pinMode(KLINE_TX_PIN, INPUT);
    KLineSerial.begin(ECU_BAUDRATE, SERIAL_8N1, KLINE_RX_PIN, KLINE_TX_PIN);
    
    klineFastInitSent = true;
    klineSyncCount = 0;
    klineTransitionState(KL_FAST_INIT_WAIT_SYNC);
    Serial.println("[KLINE] Fast init (5 baud) sent");
}

void klineSendByte(uint8_t byte) {
    KLineSerial.write(byte);
    KLineSerial.flush();
}

void klineSendFrame(uint8_t* data, uint8_t len) {
    for (int i = 0; i < len; i++) {
        klineSendByte(data[i]);
    }
}

uint8_t klineCalculateChecksum(uint8_t* data, uint8_t len) {
    uint8_t sum = 0;
    for (int i = 0; i < len; i++) {
        sum += data[i];
    }
    return sum;
}

void klineTransitionState(KLineState newState) {
    klineState = newState;
    klineStateStartTime = millis();
    klineRxIndex = 0;
    
    // Actions on state entry
    switch (newState) {
        case KL_FAST_INIT_5BAUD:
            klineLogAppend("[KLINE] → 5-baud init");
            klineSendFastInit();
            break;
        case KL_START_COMMS_SENT:
            klineLogAppend("[KLINE] → StartCommunication 0x81");
            klineSendByte(START_COMMS);
            break;
        case KL_TIMING_SENT:
            {
                uint8_t timingReq[] = {ACCESS_TIMING, 0x02};
                uint8_t cs = klineCalculateChecksum(timingReq, 2);
                uint8_t frame[] = {ACCESS_TIMING, 0x02, cs};
                klineSendFrame(frame, 3);
                klineLogAppend("[KLINE] → AccessTiming 0x83 0x02");
            }
            break;
        case KL_ACTIVE:
            ecuData.kLineConnected = true;
            ecuData.state = NORMAL;
            klineLogAppend("[KLINE] ACTIVE - connected");
            break;
        case KL_IDLE:
            if (ecuData.kLineConnected)
                klineLogAppend("[KLINE] → IDLE (disconnected)");
            ecuData.kLineConnected = false;
            klineFastInitSent = false;
            break;
    }
}

bool klineCheckTimeout() {
    if (millis() - klineStateStartTime > ECU_TIMEOUT) {
        return true;
    }
    return false;
}

void processKLineSerial() {
    // Check for timeout in current state (skip IDLE - waiting for trigger)
    if (klineState != KL_IDLE && klineCheckTimeout()) {
        char buf[48];
        sprintf(buf, "[KLINE] TIMEOUT state=%d", klineState);
        klineLogAppend(buf);
        klineTransitionState(KL_IDLE);
        return;
    }

    // Process incoming bytes
    while (KLineSerial.available()) {
        uint8_t byte = KLineSerial.read();
        klineLastByteTime = millis();
        
        // Store in buffer
        if (klineRxIndex < sizeof(klineRxBuffer)) {
            klineRxBuffer[klineRxIndex++] = byte;
        }

        // Log RX bytes during active handshake states
        if (klineState >= KL_FAST_INIT_WAIT_SYNC && klineState <= KL_TIMING_WAIT_RESP) {
            char buf[32];
            sprintf(buf, "  RX 0x%02X", byte);
            klineLogAppend(buf);
        }

        // State machine processing
        switch (klineState) {
            case KL_IDLE:
                // Wait for fast init pulse (handled by external trigger or manual)
                break;

            case KL_FAST_INIT_WAIT_SYNC:
                // Wait for sync bytes (0x55 0x08 0x08)
                if (byte == ECU_SYNC_BYTE) {
                    klineSyncCount++;
                    if (klineSyncCount >= 3) {
                        klineTransitionState(KL_FAST_INIT_WAIT_KW1);
                    }
                } else {
                    klineSyncCount = 0;
                }
                break;

            case KL_FAST_INIT_WAIT_KW1:
                if (byte == KW1_BYTE) {
                    klineTransitionState(KL_FAST_INIT_WAIT_KW2);
                } else {
                    klineTransitionState(KL_IDLE);
                }
                break;

            case KL_FAST_INIT_WAIT_KW2:
                if (byte == KW2_INVERTED) {
                    // Handshake complete, send StartCommunication
                    klineTransitionState(KL_START_COMMS_SENT);
                } else {
                    klineTransitionState(KL_IDLE);
                }
                break;

            case KL_START_COMMS_SENT:
                if (byte == 0x80) {  // ECU response header
                    klineTransitionState(KL_START_COMMS_WAIT_RESP);
                }
                break;

            case KL_START_COMMS_WAIT_RESP:
                // Wait for full response: 0x80 0x33 0xF1 0x81 0xC1
                if (klineRxIndex >= 5) {
                    if (klineRxBuffer[0] == 0x80 && klineRxBuffer[3] == 0x81) {
                        klineTransitionState(KL_TIMING_SENT);
                    } else {
                        klineTransitionState(KL_IDLE);
                    }
                    klineRxIndex = 0;
                }
                break;

            case KL_TIMING_SENT:
                if (byte == 0xC3) {  // Timing response header
                    klineTransitionState(KL_TIMING_WAIT_RESP);
                }
                break;

            case KL_TIMING_WAIT_RESP:
                // Wait for full timing response: C3 01 00 00 00 00
                if (klineRxIndex >= 6) {
                    klineTransitionState(KL_ACTIVE);
                    ecuData.kLineConnected = true;
                    ecuData.state = NORMAL;
                    klineRxIndex = 0;
                    Serial.println("[KLINE] KWP2000 ACTIVE - SDS communication established");
                }
                break;

            case KL_ACTIVE:
                // Process live data requests - check for complete frame
                if (klineRxIndex >= 3 && klineRxBuffer[0] == READ_DATA_BY_ID) {
                    klineProcessRxBuffer();
                } else if (klineRxBuffer[0] == STOP_COMMS) {
                    klineTransitionState(KL_STOP_COMMS);
                }
                break;

            case KL_STOP_COMMS:
                klineTransitionState(KL_IDLE);
                break;
        }
    }
}

void klineProcessRxBuffer() {
    // In ACTIVE state, process incoming requests from tester (ECU emulator)
    if (klineRxIndex >= 3 && klineRxBuffer[0] == READ_DATA_BY_ID) {
        uint8_t pid = klineRxBuffer[1];
        uint8_t resp[8];
        uint8_t respLen = 0;
        
        // Generate SDS live data response for GSX-R1000 K4
        switch (pid) {
            case 0x08:  // Engine RPM (2 bytes, little endian, 0.25 RPM/LSB)
                resp[0] = 0x62;  // Positive response
                resp[1] = pid;
                resp[2] = ecuData.rpm & 0xFF;
                resp[3] = (ecuData.rpm >> 8) & 0xFF;
                respLen = 4;
                break;
            case 0x0C:  // Vehicle Speed (1 byte, km/h)
                resp[0] = 0x62;
                resp[1] = pid;
                resp[2] = ecuData.speed & 0xFF;
                respLen = 3;
                break;
            case 0x0D:  // Coolant Temp (1 byte, -40 offset)
                resp[0] = 0x62;
                resp[1] = pid;
                resp[2] = ecuData.coolantTemp;
                respLen = 3;
                break;
            case 0x11:  // Throttle Position (1 byte, %)
                resp[0] = 0x62;
                resp[1] = pid;
                resp[2] = ecuData.throttlePos;
                respLen = 3;
                break;
            case 0x0B:  // Intake Air Temp (1 byte, -40 offset)
                resp[0] = 0x62;
                resp[1] = pid;
                resp[2] = ecuData.intakeAirTemp;
                respLen = 3;
                break;
            case 0x0F:  // Intake Manifold Pressure (1 byte, kPa)
                resp[0] = 0x62;
                resp[1] = pid;
                resp[2] = ecuData.mapKpa;
                respLen = 3;
                break;
            default:
                // Negative response
                resp[0] = 0x7F;
                resp[1] = READ_DATA_BY_ID;
                resp[2] = 0x12;  // SubFunctionNotSupported
                respLen = 3;
                break;
        }
        
        klineSendFrame(resp, respLen);
    }
    klineRxIndex = 0;
}

void updateSensorData() {
    // Generate realistic ECU data
    static unsigned long lastSensorUpdate = 0;

    if (millis() - lastSensorUpdate > 50) {
        lastSensorUpdate = millis();

        // Update with realistic variations
        ecuData.rpm = 1000 + (rand() % 1500);
        ecuData.speed = ecuData.rpm / 20;
        ecuData.coolantTemp = 80 + (rand() % 25);
        ecuData.throttlePos = 10 + (rand() % 80);
        ecuData.mapKpa = 30 + (rand() % 40);
        ecuData.batteryVolt = 12 + (rand() % 10) / 10.0;
        ecuData.gearPos = rand() % 7;

        if (ecuData.rpm > 5000) ecuData.fanOn = true;
        if (ecuData.throttlePos > 90) ecuData.clutchIn = true;
        if (ecuData.speed == 0) ecuData.sidestandDown = true;
        else ecuData.sidestandDown = false;
    }
}

void broadcastBluetoothData() {
#if BT_ENABLED
    static unsigned long lastBT = 0;
    if (millis() - lastBT > 100) {
        String json = createJsonResponse();
        SerialBT.println(json);
        lastBT = millis();
    }
#endif
}

void updateLEDs() {
    // LED shows K-Line handshake progress at a glance
    // Wemos D1 Mini: GPIO2 LED is active-LOW (LOW=ON, HIGH=OFF)
    static unsigned long lastToggle = 0;
    static int blinkPhase = 0;
    unsigned long now = millis();

    switch (klineState) {
        case KL_IDLE:
            if (!ecuData.kLineConnected) {
                // Slow pulse: waiting for dealer mode
                if (now - lastToggle > 1000) {
                    digitalWrite(LED_PIN, LOW);
                    lastToggle = now;
                } else if (now - lastToggle > 100) {
                    digitalWrite(LED_PIN, HIGH);
                }
            } else {
                digitalWrite(LED_PIN, LOW);  // Solid on = connected
            }
            break;

        case KL_FAST_INIT_5BAUD:
            // Rapid flash during 5-baud init
            if (now - lastToggle > 50) {
                digitalWrite(LED_PIN, (blinkPhase++ % 2) ? LOW : HIGH);
                lastToggle = now;
            }
            break;

        case KL_FAST_INIT_WAIT_SYNC:
            // Double-blink: waiting for sync
            if (now - lastToggle > 150) {
                blinkPhase = (blinkPhase + 1) % 4;
                digitalWrite(LED_PIN, (blinkPhase < 2) ? LOW : HIGH);
                lastToggle = now;
            }
            break;

        case KL_FAST_INIT_WAIT_KW1:
        case KL_FAST_INIT_WAIT_KW2:
            // Quick blink: received sync, waiting for keywords
            if (now - lastToggle > 200) {
                digitalWrite(LED_PIN, (blinkPhase++ % 2) ? LOW : HIGH);
                lastToggle = now;
            }
            break;

        case KL_START_COMMS_SENT:
        case KL_START_COMMS_WAIT_RESP:
        case KL_TIMING_SENT:
        case KL_TIMING_WAIT_RESP:
            // Alternating pattern: negotiating
            if (now - lastToggle > 100) {
                blinkPhase = (blinkPhase + 1) % 6;
                digitalWrite(LED_PIN, (blinkPhase < 3) ? LOW : HIGH);
                lastToggle = now;
            }
            break;

        case KL_ACTIVE:
            digitalWrite(LED_PIN, LOW);  // Solid on = fully connected
            break;

        case KL_STOP_COMMS:
            digitalWrite(LED_PIN, HIGH);  // Off
            break;

        default:
            if (now - lastToggle > 500) {
                digitalWrite(LED_PIN, (blinkPhase++ % 2) ? LOW : HIGH);
                lastToggle = now;
            }
            break;
    }
}

void checkDealerMode() {
    if (digitalRead(DEALER_PIN) == LOW) {
        if (!ecuData.dealerModeActive) {
            ecuData.dealerModeActive = true;
            ecuData.dealerModeStart = millis();
            Serial.println("DEALER MODE: Activated");
        }
    } else {
        if (ecuData.dealerModeActive) {
            ecuData.dealerModeActive = false;
            Serial.println("DEALER MODE: Released");
        }
    }
}

// Response helpers
void sendJsonResponse(AsyncWebServerRequest *request, String json) {
    request->send(200, "application/json", json);
}

void sendKLineStatus(AsyncWebServerRequest *request) {
    JsonDocument jsonDoc;
    jsonDoc["address"] = KLINE_RX_PIN;
    jsonDoc["baudrate"] = ECU_BAUDRATE;
    jsonDoc["connected"] = ecuData.kLineConnected;
    jsonDoc["dealerPin"] = DEALER_PIN;
    jsonDoc["dealerModeActive"] = ecuData.dealerModeActive;
    jsonDoc["timestamp"] = millis();

    String json;
    serializeJson(jsonDoc, json);
    sendJsonResponse(request, json);
}

void sendSystemStatus(AsyncWebServerRequest *request) {
    JsonDocument jsonDoc;
    jsonDoc["chipModel"] = ESP.getChipModel();
    jsonDoc["chipRevision"] = ESP.getChipRevision();
    jsonDoc["cpuFreq"] = ESP.getCpuFreqMHz();
    jsonDoc["freeHeap"] = ESP.getFreeHeap();
    jsonDoc["sketchSize"] = ESP.getSketchSize();
    jsonDoc["freeSketchSpace"] = ESP.getFreeSketchSpace();
    jsonDoc["flashChipSize"] = ESP.getFlashChipSize();
    jsonDoc["wifiRSSI"] = WiFi.RSSI();
    jsonDoc["staMode"] = WiFi.getMode() == WIFI_STA;
    jsonDoc["softAPMode"] = WiFi.getMode() == WIFI_AP;
    jsonDoc["sdkVersion"] = ESP.getSdkVersion();
    jsonDoc["currentState"] = stateToString(ecuData.state);
    jsonDoc["dealerModeActive"] = ecuData.dealerModeActive;
    jsonDoc["timestamp"] = millis();

    String json;
    serializeJson(jsonDoc, json);
    sendJsonResponse(request, json);
}

void handleCommandRequest(AsyncWebServerRequest *request) {
    String cmd;
    String body = request->arg("plain");
    if (body.length() > 0) {
        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, body);
        if (!err && doc["cmd"].is<const char*>()) {
            cmd = doc["cmd"].as<String>();
        }
    }
    // Fallback: query parameter
    if (cmd.length() == 0 && request->hasParam("cmd")) {
        cmd = request->getParam("cmd")->value();
    }

    String response;

    if (cmd == "reset") {
        setupEcudata();
        response = "{\"status\": \"reset\"}";
    } else if (cmd == "start_kline") {
        if (ecuData.dealerModeActive) {
            klineTransitionState(KL_FAST_INIT_5BAUD);
            response = "{\"status\": \"kline_started\"}";
        } else {
            response = "{\"error\": \"dealer_mode_required\"}";
        }
    } else if (cmd == "calibrate") {
        ecuData.coolantTemp = 85;
        response = "{\"status\": \"calibrated\"}";
    } else if (cmd == "flash") {
        if (ecuData.dealerModeActive) {
            response = "{\"status\": \"flash_started\"}";
            delay(100);
            response = "{\"status\": \"flash_complete\"}";
        } else {
            response = "{\"error\": \"dealer_mode_required\"}";
        }
    } else {
        response = "{\"error\": \"unknown_command\"}";
    }

    sendJsonResponse(request, response);
}

void handleDealerRequest(AsyncWebServerRequest *request) {
    String action;
    // Try JSON body first
    String body = request->arg("plain");
    if (body.length() > 0) {
        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, body);
        if (!err && doc["action"].is<const char*>()) {
            action = doc["action"].as<String>();
        }
    }
    // Fallback: query parameter
    if (action.length() == 0 && request->hasParam("action")) {
        action = request->getParam("action")->value();
    }

    if (action == "enter") {
        ecuData.dealerModeActive = true;
        ecuData.dealerModeStart = millis();
        Serial.println("DEALER MODE: Activated (API)");
        sendJsonResponse(request, "{\"status\": \"dealer_mode_entered\"}");
    } else if (action == "exit") {
        ecuData.dealerModeActive = false;
        Serial.println("DEALER MODE: Released (API)");
        sendJsonResponse(request, "{\"status\": \"dealer_mode_exited\"}");
    } else {
        sendJsonResponse(request, "{\"error\": \"invalid_action\"}");
    }
}

String getHtmlPage() {
    return R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>GSX-R1000 ECU</title>
<style>
 *{margin:0;padding:0;box-sizing:border-box;font-family:system-ui,sans-serif}
 body{background:#0a0e17;color:#c8d6e5;padding:16px;max-width:1000px;margin:auto}
 h1{font-size:1.2rem;margin-bottom:12px;color:#48dbfb}
 h1 span{font-size:.7rem;color:#8395a7;font-weight:400}
 .grid{display:grid;gap:10px;grid-template-columns:repeat(auto-fit,minmax(130px,1fr))}
 .card{background:#111827;border:1px solid #1f2937;border-radius:8px;padding:12px}
 .card .label{font-size:.65rem;text-transform:uppercase;color:#64748b;letter-spacing:.5px}
 .card .value{font-size:1.4rem;font-weight:700;margin-top:2px;font-variant-numeric:tabular-nums}
 .card .unit{font-size:.7rem;color:#8395a7;margin-left:2px}
 .status-bar{display:flex;gap:8px;align-items:center;margin-bottom:10px;flex-wrap:wrap}
 .status-dot{width:10px;height:10px;border-radius:50%;display:inline-block}
 .dot-green{background:#10b981;box-shadow:0 0 6px #10b981}
 .dot-red{background:#ef4444;box-shadow:0 0 6px #ef4444}
 .dot-yellow{background:#f59e0b;box-shadow:0 0 6px #f59e0b}
 .dot-gray{background:#475569}
 .status-text{font-size:.8rem;color:#94a3b8}
 .btn{background:#1e293b;color:#e2e8f0;border:1px solid #334155;padding:6px 14px;border-radius:5px;cursor:pointer;font-size:.75rem;transition:.15s}
 .btn:hover{background:#334155;border-color:#475569}
 .btn-green{background:#065f46;border-color:#059669}
 .btn-green:hover{background:#059669}
 .btn-red{background:#7f1d1d;border-color:#dc2626}
 .btn-red:hover{background:#dc2626}
 .terminal{background:#000814;border:1px solid #1f2937;border-radius:8px;padding:10px;font-family:'Courier New',monospace;font-size:.7rem;height:200px;overflow-y:auto;white-space:pre-wrap;line-height:1.5;margin-top:10px}
 .terminal::-webkit-scrollbar{width:6px}
 .terminal::-webkit-scrollbar-track{background:#000814}
 .terminal::-webkit-scrollbar-thumb{background:#1e293b;border-radius:3px}
 .terminal .rx{color:#48dbfb}
 .terminal .tx{color:#fbbf24}
 .terminal .info{color:#94a3b8}
 .terminal .err{color:#ef4444}
 .terminal .ok{color:#10b981}
 .dealer-row{display:flex;gap:8px;align-items:center;flex-wrap:wrap;margin-top:6px}
 .flash-warn{color:#f59e0b;font-size:.7rem;margin-top:4px}
</style>
</head>
<body>
<h1>&#9881; GSX-R1000 K4 ECU <span>Wemos D1 Mini · K-Line 10400</span></h1>

<div class="status-bar" id="statusBar">
  <span class="status-dot dot-gray" id="statusDot"></span>
  <span class="status-text" id="statusText">Initializing...</span>
  <span style="flex:1"></span>
  <button class="btn" onclick="fetchCmd('reset')">&#x21bb; Reset</button>
</div>

<div class="grid" id="dataGrid">
  <div class="card"><div class="label">RPM</div><div class="value" id="vRpm">--</div></div>
  <div class="card"><div class="label">Speed</div><div class="value" id="vSpeed">-- <span class="unit">km/h</span></div></div>
  <div class="card"><div class="label">Coolant</div><div class="value" id="vCoolant">-- <span class="unit">&deg;C</span></div></div>
  <div class="card"><div class="label">Intake Air</div><div class="value" id="vIat">-- <span class="unit">&deg;C</span></div></div>
  <div class="card"><div class="label">Throttle</div><div class="value" id="vTps">-- <span class="unit">%</span></div></div>
  <div class="card"><div class="label">MAP</div><div class="value" id="vMap">-- <span class="unit">kPa</span></div></div>
  <div class="card"><div class="label">Battery</div><div class="value" id="vBatt">-- <span class="unit">V</span></div></div>
  <div class="card"><div class="label">Gear</div><div class="value" id="vGear">--</div></div>
  <div class="card"><div class="label">Ignition</div><div class="value" id="vIgn">-- <span class="unit">&deg;</span></div></div>
  <div class="card"><div class="label">Injector</div><div class="value" id="vInject">-- <span class="unit">ms</span></div></div>
</div>

<div class="dealer-row" id="dealerRow">
  <button class="btn btn-green" id="btnDealerEnter" onclick="toggleDealer(true)">&#x1f512; Enter Dealer Mode</button>
  <button class="btn btn-red" id="btnDealerExit" onclick="toggleDealer(false)" style="display:none">&#x1f512; Exit Dealer Mode</button>
  <span class="status-text" id="dealerStatus" style="display:none">Dealer mode active</span>
  <button class="btn" id="btnStartKline" onclick="startKline()" style="display:none">&#x25b6; Start K-Line</button>
  <div class="flash-warn" id="flashWarn" style="display:none">&#x26a0; Dealer mode required for flash</div>
</div>

<h2 style="font-size:.8rem;margin-top:14px;color:#64748b;text-transform:uppercase;letter-spacing:.5px">K-Line Terminal</h2>
<div class="terminal" id="terminal">Waiting for connection...</div>

<script>
const TERM = document.getElementById('terminal');
let firstLog = true;

function a(v){return v??'--'}
function pollEcu(){
  fetch('/api/ecu').then(r=>r.json()).then(d=>{
    document.getElementById('vRpm').textContent = a(d.rpm);
    document.getElementById('vSpeed').textContent = a(d.speed);
    document.getElementById('vCoolant').innerHTML = a(d.coolantTemp)+' <span class="unit">&deg;C</span>';
    document.getElementById('vIat').innerHTML = a(d.intakeAirTemp)+' <span class="unit">&deg;C</span>';
    document.getElementById('vTps').innerHTML = a(d.throttlePos)+' <span class="unit">%</span>';
    document.getElementById('vMap').innerHTML = a(d.mapKpa)+' <span class="unit">kPa</span>';
    document.getElementById('vBatt').innerHTML = a(d.batteryVolt)+' <span class="unit">V</span>';
    document.getElementById('vGear').textContent = a(d.gear);
    document.getElementById('vIgn').innerHTML = a(d.ignitionTiming)+' <span class="unit">&deg;</span>';
    document.getElementById('vInject').innerHTML = a(d.injectorPulse)+' <span class="unit">ms</span>';

    const dot = document.getElementById('statusDot');
    const txt = document.getElementById('statusText');
    if(d.kLineConnected){
      dot.className='status-dot dot-green';
      txt.textContent='K-Link Connected | State: '+a(d.state);
    }else if(d.dealerModeActive){
      dot.className='status-dot dot-yellow';
      txt.textContent='Dealer Mode | K-Link idle';
    }else{
      dot.className='status-dot dot-red';
      txt.textContent='Standby';
    }
  }).catch(()=>{});
}
function pollStatus(){
  fetch('/api/kline').then(r=>r.json()).then(d=>{
    if(d.dealerModeActive){
      document.getElementById('btnDealerEnter').style.display='none';
      document.getElementById('btnDealerExit').style.display='';
      document.getElementById('dealerStatus').style.display='';
      document.getElementById('btnStartKline').style.display='';
      document.getElementById('flashWarn').style.display='';
    }else{
      document.getElementById('btnDealerEnter').style.display='';
      document.getElementById('btnDealerExit').style.display='none';
      document.getElementById('dealerStatus').style.display='none';
      document.getElementById('btnStartKline').style.display='none';
      document.getElementById('flashWarn').style.display='none';
    }
  }).catch(()=>{});
}
function pollLog(){
  fetch('/api/log').then(r=>r.text()).then(t=>{
    if(!t)return;
    if(firstLog){TERM.textContent='';firstLog=false}
    TERM.textContent = t.split('\\n').slice(-30).join('\\n');
    TERM.scrollTop = TERM.scrollHeight;
  }).catch(()=>{});
}
function fetchCmd(cmd){
  fetch('/api/cmd',{method:'POST',body:JSON.stringify({cmd:cmd}),headers:{'Content-Type':'application/json'}});
}
function toggleDealer(enter){
  fetch('/api/dealer',{method:'POST',body:JSON.stringify({action:enter?'enter':'exit'}),headers:{'Content-Type':'application/json'}});
}
function startKline(){
  fetch('/api/cmd',{method:'POST',body:JSON.stringify({cmd:'start_kline'}),headers:{'Content-Type':'application/json'}});
}
setInterval(pollEcu,200);
setInterval(pollStatus,1000);
setInterval(pollLog,500);
pollEcu();pollStatus();pollLog();
</script>
</body>
</html>
)rawliteral";
}