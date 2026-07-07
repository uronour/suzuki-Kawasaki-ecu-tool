# ESP32 K-Line Firmware Flashing with Arduino Uno

## Required Hardware

### ESP32 Module
- **ESP32 DevKitC** (recommended) or **ESP32-01**
- USB to TTL serial adapter on board (already has CP2102/CH340)

### Arduino Uno as Programmer/Interface
- Arduino Uno (or Nano)
- USB-A to USB-B cable for Uno
- Optional: USB-to-DB9 serial adapter

### Connection Diagram (ESP32 DevKitC)

```
ESP32 DevKitC          Arduino Uno
─────────────────    ───────────────────
3.3V                → VIN (5V on Uno)  
GND                → GND
TX (GPIO21)         → D0 (UNO RX)  
RX (GPIO20)         → D1 (UNO TX)
EN (GPIO0)          → Not connected (floating)
IO0 (GPIO0)         → Not connected (floating)
```

**Alternative (using USB-to-DB9 adapter):**
```
ESP32 TX (GPIO21) ──> DB9 Rx
ESP32 RX (GPIO20) ──> DB9 Tx
ESP32 GND ─────> DB9 GND
```

## Step-by-Step Flashing Instructions

### Step 1: Install Dependencies

**For ESP32 support in Arduino IDE:**
```
1. Download Arduino IDE (latest version)
2. Open Arduino IDE
3. File → Preferences:
   - Additional Board Manager URLs:
     https://raw.githubusercontent.com/espressif/esp32-mcu-pkg/gh-pages/package_esp32_index.json

4. Tools → Boards Manager:
   - Search "ESP32"
   - Install "ESP32 by Espressif"
```

### Step 2: Prepare ESP32

**For ESP32 DevKitC / Node-32S:**
```
1. Power ON ESP32
2. GPIO0 (EN pin) → leave unconnected (floating)
3. No special setup needed before flashing
```

**For ESP32-01 (cheaper module):**
```
1. Power OFF ESP32
2. Connect GPIO0 → GND
3. Power ON ESP32
4. AFTER startup begins → disconnect GPIO0 from GND
```

### Step 3: Open Arduino IDE

```
1. File → New (or open existing project)
2. Add the ESP32 K-Line firmware code
3. Tools → Board: [Select "ESP32 DevKitC" or "ESP32-01"]
4. Tools → Flash Size: [Select "4MB (AutoDetect)"]
```

### Step 4: Configure for Arduino Uno Port

```
1. Tools → Port: [Select COM port for Arduino Uno]
   Example: COM3, COM5, or whatever your Uno shows

2. Verify connection:
   - Check Arduino IDE Serial Monitor
   - Should show "ESP32 booting..."
```

### Step 5: Upload Firmware

```
1. Press Upload button (right arrow) in Arduino IDE
2. Wait for compilation and flashing
3. Monitor Serial Monitor for progress dots
4. Look for these messages:
   - "\.\.\." (dots during flashing)
   - "Done uploading"
   - Boot messages: "ESP32 boot...", "Starting..."
```

## Example Wiring Diagram

**USB-to-DB9 Serial Adaptor (Easiest Method)**

```
                  ESP32 DevKitC        USB-to-DB9 Adapter         Arduino Uno
                   ──────────────────   ──────────────────    ──────────────────
                    TX (GPIO21)  ────>  DB9 Rx Pin         D0 ────> Rx on Uno
                    RX (GPIO20)  ────>  DB9 Tx Pin         D1 ────> Tx on Uno  
                    GND         ────>  DB9 GND Pin         GND ────> GND
                    EN (GPIO0)  ────>  Not connected      (leave floating)
                    IO0 (GPIO0)  ────>  Not connected      (leave floating)
```

## Step-by-Step Detailed Instructions

### Detailed Connection Steps:

**Step 1: Setup ESP32**  
```
1. Power on ESP32 (if using separate power source)
2. Leave GPIO0 unconnected (floating)
3. For ESP32-01 variant: connect GPIO0 to GND before power-on
```

**Step 2: Connect Arduino Uno**  
```
1. Connect USB cable to Arduino Uno
2. Open Arduino IDE
3. Wait for COM port to be recognized
```

**Step 3: Configure Arduino IDE**  
```
Tools → Board: [Select your ESP32 model]
Tools → Port: [Select COM port showing in Arduino IDE]
Tools → Flash Size: 4MB (or AutoDetect)
```

**Step 4: Open Source Code**  
```
File → Open → [Navigate to your ESP32 K-Line firmware]
```

**Step 5: Install Library (if needed)**  
```
For ESP32, no special libraries needed
For ArduinoJson, add via Library Manager:
- Library → Manage Libraries
- Search ArduinoJson → Install
```

**Step 6: Upload**  
```
Click the Upload button in Arduino IDE toolbar
```

### Alternative: Using USB-to-DB9 Adapter

**If direct UART-to-USB connection fails:**

```
1. Connect DB9 Rx → D0 on Uno
2. Connect DB9 Tx → D1 on Uno  
3. Connect DB9 GND → GND on Uno
4. Modify Arduino IDE port settings if needed
```

## Troubleshooting Common Issues

### **Problem: Upload fails**
```
\u2022 ESP32 not in boot mode
\u2022 GPIO0 incorrectly connected
\u2022 Wrong COM port
\u2022 ESP32 power issues
Solution:
- \u2022 For ESP32-01: connect GPIO0 to GND, then disconnect after startup
- \u2022 Try different USB port on computer
- \u2022 Restart both Arduino Uno and ESP32
```

### **Problem: No output in Serial Monitor**
```
\u2022 Baud rate mismatch
\u2022 Wiring errors
\u2022 Power issues
Solution:
- \u2022 Try 115200, 921600, or 256000 baud rates
- \u2022 Double-check all connections
- \u2022 Ensure ESP32 has power
```

### **Problem: Upload timeout**
```
\u2022 Slow computer or USB issues
\u2022 Corrupted firmware
\u2022 Board configuration issues
Solution:
- \u2022 Try different USB cable
- \u2022 Use a faster computer
- \u2022 Check board configuration
```

## Expected Output During Upload

During the upload process, you should see:

```
began upload
.... (dots showing progress)
done uploading

\n=== Startup ===
ESP32 boot...
Starting...
WiFi AP: GSX-R1000_ECU
Dealer Mode Pin: GPIO12
K-Line Baudrate: 10400
Bluetooth Name: GSX-R1000_ECU

System initialized - waiting for connections...
```

## Step 7: Verify Successful Flash

**After upload completes successfully:**

```
1. Open Arduino IDE Serial Monitor
2. Look for "ESP32 K-Line Bridge" message
3. Check "WiFi AP: GSX-R1000_ECU"
4 snif the "OTA enabled - http://[IP]/setup"
4. ESP32 should be ready to accept connections
```

## Alternative Programming: Using esptool

**If Arduino IDE doesn't work:**

```bash
# Install esptool (Python package)
pip install esptool

# Flashing command (adjust paths as needed)
esptool.py --chip esp32 write_flash 0x0 firmware.bin
```

**Note:** esptool requires exact binary firmware, not source code.

## Quick Testing Commands

**After successful flash:**

```bash
# Test WiFi connection (with ESP32 IP)
curl http://192.168.4.1/api/ecu

# Test Bluetooth
# Use Bluetooth scanner app on phone

# Test dealer mode (POST)
curl -X POST http://192.168.4.1/api/dealer -H "Content-Type: application/json" -d '{"action": "enter"}'
```

## Step 8: Integration with Your System

**After successful flash:**

```
1. ESP32 is ready to communicate
2. Connect to WiFi (GSX-R1000_ECU network)
3. Use REST APIs for ECU data
4. Test Bluetooth connection
5. Integrate with your Android/Windows apps
```

## Troubleshooting Summary

**Most Common Issues:**
- **Upload fails:** Check ESP32 boot mode and connections
- **No output:** Verify baud rate and wiring
- **Timeout:** Try different USB/port combination
- **Authentication failed:** Retry with correct credentials

**Quick Fix Commands:**
```bash
# Restart all devices
# Try different USB ports
# Check connections
# Restart Arduino IDE
```

## Final Verification

**Once flashed successfully:**
```
✓ ESP32 should boot and display startup messages
✓ WiFi network created (GSX-R1000_ECU)
✓ OTA updates enabled
✓ Bluetooth advertising
✓ K-Line protocol ready
✓ Dealer mode pin operational
✓ REST APIs accessible
```

## Next Steps

**After successful flash:**
1. Test basic connectivity (WiFi, Bluetooth)
2. Verify API endpoints
3. Test with your PC/Android apps
4. Implement monitoring/logging
5. Set up OTA update testing

This setup gives you a complete ESP32 K-Line device with:

- ✅ WiFi for PC tool connections
- ✅ Bluetooth for Android app
- ✅ K-Line communication for ECU data
- ✅ Dealer mode for special functions
- ✅ OTA update capability
- ✅ REST APIs for integration

The ESP32 version is smaller and more cost-effective than the STM32 setup while providing all the ECU communication capabilities you need.