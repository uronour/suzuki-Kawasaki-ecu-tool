/*
 * sds_esp32_web.ino — ESP32 WiFi Web Dashboard
 *
 * Serves a real-time gauge dashboard over WiFi — access from any phone.
 * ESP32 creates an access point named "SDS-Reader".
 * Open browser to http://192.168.4.1
 *
 * Modes: Bluetooth SPP + WiFi AP simultaneously
 *
 * Hardware: ESP32 + L9637D + 4N35
 */

#include "SDSProtocol.h"
#include <WiFi.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <ArduinoJson.h>

// WiFi AP credentials
const char *ssid = "SDS-Reader";
const char *password = "suzuki123";

// Web server
WebServer server(80);
WebSocketsServer webSocket(81);

// K-Line
HardwareSerial kline(2);
SDSProtocol ecu(&kline, 17, 4);

// Sensor cache
char json_buffer[512];
unsigned long last_sensor_ms = 0;

// HTML page stored in PROGMEM
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta name="viewport" content="width=320, initial-scale=1">
<style>
  * { margin: 0; padding: 0; box-sizing: border-box; }
  body { font-family: 'Segoe UI', sans-serif; background: #0a0a1a; color: #fff; padding: 10px; }
  .gauge { background: #1a1a3e; border-radius: 10px; padding: 10px; margin: 5px 0; display: flex; justify-content: space-between; align-items: center; }
  .gauge .label { font-size: 12px; color: #888; }
  .gauge .value { font-size: 24px; font-weight: bold; font-family: monospace; }
  .rpm { color: #ff4444; }
  .green { color: #44ff44; }
  .yellow { color: #ffaa00; }
  .cyan { color: #00ffff; }
  .bar-container { background: #333; border-radius: 5px; height: 8px; width: 100%; margin: 5px 0; }
  .bar-fill { height: 8px; border-radius: 5px; transition: width 0.2s; }
  #rpm-bar { background: linear-gradient(90deg, #00ff00, #ffff00, #ff0000); }
  #tps-bar { background: #0088ff; }
  h1 { text-align: center; font-size: 18px; color: #ff4444; margin-bottom: 10px; }
  .status { text-align: center; font-size: 11px; color: #666; margin-top: 10px; }
</style>
</head>
<body>
<h1>🏍 SDS Reader — GSX-R1000</h1>

<div class="gauge">
  <span class="label">RPM</span>
  <span class="value rpm" id="rpm">0</span>
</div>
<div class="bar-container"><div class="bar-fill" id="rpm-bar" style="width:0%"></div></div>

<div class="gauge">
  <span class="label">Speed</span>
  <span class="value green"><span id="speed">0</span> km/h</span>
</div>

<div class="gauge">
  <span class="label">Gear</span>
  <span class="value yellow" id="gear">N</span>
</div>

<div class="gauge">
  <span class="label">TP Sensor</span>
  <span class="value cyan"><span id="tps">0</span>%</span>
</div>
<div class="bar-container"><div class="bar-fill" id="tps-bar" style="width:0%"></div></div>

<div class="gauge">
  <span class="label">Coolant</span>
  <span class="value green"><span id="ect">0</span>°C</span>
</div>

<div class="gauge">
  <span class="label">Intake Air</span>
  <span class="value cyan"><span id="iat">0</span>°C</span>
</div>

<div class="gauge">
  <span class="label">MAP</span>
  <span class="value yellow"><span id="map">0</span> kPa</span>
</div>

<div class="gauge">
  <span class="label">Battery</span>
  <span class="value" id="batt">0.0V</span>
</div>

<div class="gauge">
  <span class="label">Ignition Angle</span>
  <span class="value cyan"><span id="ign">0</span>° BTDC</span>
</div>

<div class="status" id="status">Connected</div>

<script>
var ws = new WebSocket('ws://' + location.hostname + ':81');
ws.onmessage = function(e) {
  var d = JSON.parse(e.data);
  document.getElementById('rpm').textContent = d.rpm;
  document.getElementById('speed').textContent = d.speed;
  document.getElementById('gear').textContent = d.gear == 0 ? 'N' : d.gear;
  document.getElementById('tps').textContent = d.tps;
  document.getElementById('ect').textContent = d.ect;
  document.getElementById('iat').textContent = d.iat;
  document.getElementById('map').textContent = d.map;
  document.getElementById('batt').textContent = d.batt + 'V';
  document.getElementById('ign').textContent = d.ign;
  document.getElementById('rpm-bar').style.width = Math.min(100, d.rpm / 140) + '%';
  document.getElementById('tps-bar').style.width = d.tps + '%';
  var batt = document.getElementById('batt');
  batt.style.color = d.batt > 12 ? '#44ff44' : (d.batt > 11 ? '#ffaa00' : '#ff4444');
};
</script>
</body>
</html>
)rawliteral";

void setup() {
    Serial.begin(115200);

    // WiFi AP
    WiFi.softAP(ssid, password);
    IPAddress ip = WiFi.softAPIP();
    Serial.print(F("AP IP: "));
    Serial.println(ip);

    // Web server
    server.on("/", []() {
        server.send_P(200, "text/html", index_html);
    });
    server.begin();

    // WebSocket
    webSocket.begin();

    // K-Line
    pinMode(4, OUTPUT);
    digitalWrite(4, LOW);
    if (ecu.begin()) {
        Serial.println(F("ECU connected"));
        ecu.set_dealer_mode(true);
    } else {
        Serial.println(F("ECU init failed"));
    }
}

void loop() {
    server.handleClient();
    webSocket.loop();

    // Poll sensors and broadcast
    if (ecu.is_connected() && millis() - last_sensor_ms > 100) {
        last_sensor_ms = millis();
        ecu.request_sensors();
        ecu.keep_alive();

        StaticJsonDocument<256> doc;
        doc["rpm"] = ecu.rpm;
        doc["speed"] = ecu.speed;
        doc["gear"] = ecu.gear;
        doc["tps"] = ecu.tps;
        doc["ect"] = ecu.ect;
        doc["iat"] = ecu.iat;
        doc["map"] = ecu.map_kpa;
        doc["batt"] = round(ecu.battery_v * 10) / 10.0;
        doc["ign"] = ecu.ignition_angle;

        serializeJson(doc, json_buffer);
        webSocket.broadcastTXT(json_buffer);
    }
}
