# ESP32 K-Line Bridge - VS Code PlatformIO Workflow

## Quick Start in VS Code

### 1. Open Project
```
File → Open Folder → C:\Users\urono\Documents\ecu\suzuki-ecu-tool\firmware\esp32_kline
```

### 2. Install PlatformIO Extension
```
Extensions (Ctrl+Shift+X) → Search "PlatformIO IDE" → Install
Restart VS Code when prompted
```

### 3. Build & Flash Workflow

**Option A: Command Palette (Ctrl+Shift+P)**
```
PlatformIO: Build          → Compiles firmware
PlatformIO: Upload         → Flashes to ESP32 on COM5
PlatformIO: Monitor        → Opens serial monitor (115200 baud)
PlatformIO: Upload and Monitor → Flash + monitor in one step
```

**Option B: Terminal**
```bash
pio run -e esp32dev                    # Build only
pio run -e esp32dev -t upload          # Build + Flash
pio run -e esp32dev -t monitor         # Serial monitor
pio run -e esp32dev -t upload -t monitor  # Flash + Monitor
pio run -e esp32dev -t clean           # Clean build
pio run -e esp32dev -t size            # Show memory usage
```

### 4. VS Code Tasks (Terminal → Run Task)
- `PlatformIO: Build` - Compile firmware
- `PlatformIO: Upload` - Flash to ESP32
- `PlatformIO: Monitor` - Serial monitor
- `PlatformIO: Upload and Monitor` - All-in-one
- `PlatformIO: Clean` - Clean build
- `PlatformIO: Size Info` - Memory usage

## Hardware Setup

### ESP32 DevKitC / Node-32S
```
ESP32 DevKitC          Arduino Uno (as USB-Serial)
──────────────────     ────────────────────────
TX (GPIO21)     ──>    D0 (Rx)
RX (GPIO20)     ──>    D1 (Tx)
GND             ──>    GND
5V / VIN        ──>    5V
EN (GPIO0)      →      Not connected (floating)
```

### Arduino Uno Setup
1. Remove ATmega328 chip, OR
2. Hold RESET on Uno (jumper RST→GND)
3. Connect USB cable to Uno → PC (shows as COM5)

### ESP32-01 (Cheaper Module)
```
ESP32-01              Arduino Uno
──────────────        ───────────────────
GPIO0 (IO0)     →     GND (before power-on)
TX (GPIO21)     ──>   D0 (Rx)
RX (GPIO20)     ──>   D1 (Tx)
GND             ──>   GND
VCC             ──>   3.3V regulator
```
**Important**: Remove GPIO0→GND after power-on for normal boot.

## Project Structure
```
esp32_kline/
├── platformio.ini          # PlatformIO config
├── src/
│   └── main.cpp            # Main firmware
├── .vscode/
│   ├── launch.json         # Debug configs
│   └── tasks.json          # Build tasks
├── README.md               # This file
└── README_FLASH.md         # Detailed flashing guide
```

## Expected Output After Flash

### Serial Monitor (115200 baud):
```
=== ESP32 K-Line Bridge ===
WiFi AP: GSX-R1000_ECU
Password: suzuki2004
Dealer Mode Pin: GPIO12
K-Line Baudrate: 10400
Bluetooth Name: GSX-R1000_ECU

System initialized - waiting for connections...
```

### Web Interface
```
Browser → http://192.168.4.1
```

### API Endpoints
```
GET  /api/ecu        - JSON ECU data
POST /api/cmd        - Send commands
POST /api/dealer     - Toggle dealer mode
GET  /api/kline      - K-Line status
GET  /api/status     - System status
```

## Troubleshooting

### Build Fails
```bash
pio run -e esp32dev -t clean
pio run -e esp32dev
```

### Upload Fails (COM5)
```bash
# Check port
pio device list

# Try different baud
pio run -e esp32dev -t upload --upload-port COM5 --upload-speed 115200

# Force boot mode
# ESP32: GPIO0→GND, power cycle, release GPIO0, upload
```

### No Serial Output
```bash
# Different baud rates to try
pio device monitor -e esp32dev --baud 115200
pio device monitor -e esp32dev --baud 921600
pio device monitor -e esp32dev --baud 256000
```

### ESP32 Not in Boot Mode
```
1. GPIO0 → GND (for ESP32-01)
2. Power cycle ESP32
3. Release GPIO0 after power-on
4. Quickly click Upload
```

## Memory Usage
```bash
pio run -e esp32dev -t size
# Expected: ~500KB flash, ~50KB RAM
```

## Key Features in Firmware
- **K-Line ISO9141-2** @ 10,400 baud (HardwareSerial)
- **5-baud slow init** for ECU connection
- **WiFi AP** (GSX-R1000_ECU / suzuki2004)
- **Bluetooth SPP** (GSX-R1000_ECU)
- **Dealer Mode** (GPIO12 - long press)
- **OTA Updates** (http://192.168.4.1/setup)
- **REST API** (/api/ecu, /api/cmd, etc.)
- **ECU Data Simulation** (RPM, temps, gear, etc.)

## Target ECUs
- 2004 Suzuki GSX-R1000 (32920-18G20)
- 2004 Kawasaki ZX-10R (K-Line ISO9141-2)

## Next Steps
1. Open folder in VS Code
2. Build (Ctrl+Shift+B)
3. Connect hardware
4. Upload (Terminal → Run Task → PlatformIO: Upload)
5. Monitor (Terminal → Run Task → PlatformIO: Monitor)