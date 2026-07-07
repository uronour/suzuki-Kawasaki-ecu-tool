# AGENTS.md — Suzuki SDS ECU Tool

## Project Goal

Replace the Suzuki factory dealer tool with a custom handheld diagnostic unit for the 2004 GSX-R1000 (Denso 32920-18G20 SDS ECU). Displays real-time sensor data (RPM, speed, temperatures, TPS, battery voltage, gear position, etc.).

Two development tracks:

| Track | Target | Purpose |
|-------|--------|---------|
| **STM32 TFT** | STM32F207VCT6 + ILI9488 | Full handheld diagnostic tool |
| **K-Line Bridge** | Wemos D1 Mini ESP32 + ISO9141 Click | Production K-Line bridge with WiFi AP + Bluetooth + web dashboard |

---

## Track 1: STM32 Handheld Diagnostic Tool

### Hardware

| Feature | Spec |
|---------|------|
| MCU | STM32F207VCT6 (Cortex-M3, 120MHz) |
| SRAM | 96KB internal (no external RAM) |
| Flash | 224KB after bootloader (0x08008000 - 0x0803FFFF) |
| Display | ILI9488 480×320 RGB565, FSMC 16-bit parallel @0x60000000 |
| Touch | XPT2046, bit-banged SPI (PE3-6, PC13 IRQ) |
| USART1 | WiFi port → USR-C322 WiFi module (TCP bridge) |
| USART2 | RS232 port → USB-TTL debug console |
| USART3 | DIY port → ISO 9141 Click (L9637 K-Line) |
| Encoder | PA8/PC9/PC8 rotary encoder |
| SD card | SPI1 for data logging |

### Firmware Components

| Component | Files | Description |
|-----------|-------|-------------|
| DMA Framebuffer | `sds_display.c/h` | 32KB partial framebuffer (480×34 lines), DMA2 Stream5 memory-to-memory flush |
| Touch | `xpt2046.c` | Non-blocking state machine: IDLE→MEASURE→AVG→WAIT_RELEASE, TIM7 timer-based |
| Gauge Needle | `gauge_dial.c` | Anti-aliased needle with sub-pixel alpha blend, tapered with shadow, 1° sin/cos LUT |
| Dirty Rect | `dirty_rect.c/h` | 32-entry rect merge array, integrated with display flush |
| WiFi | USART1+USR-C322 | TCP server on port 9999, AT config at 115200 baud |
| TCP Bridge | `serial_bridge.c` + `bridge_protocol.py` | Text/binary command relay over WiFi↔UART |
| SDL2 Simulator | `sim_main.c`, `sim_hal.c` | Full mock STM32 HAL for PC testing, 2× scale SDL2 window |

### Page System
- 6 pages: Dashboard, Sensors, DTC, Status, About, Settings
- Touch: left half→prev, right half→next. Encoder: 2 clicks→change, press→confirm
- Auto-cycle 30s idle → 10s per page

### Build
- PlatformIO (CMSIS, STM32F207VCT6): ~46KB flash, ~37KB RAM
- PC tool: `cd python/sds_tool && python -m PyInstaller sds_tool.spec`
- Simulator: `gcc -DSIMULATOR -DTFT35_V3_0`

### Known STM32 Issues
1. Gauge face redraw every frame (no double-buffer) — needle-erasure arc corruption
2. External SRAM (IS62WV51216, 1MB) not fitted — would eliminate tile-line artifacts
3. No K-Line hardware testing yet on this platform

---

## Track 2: K-Line KWP2000 Bridge Firmware (Wemos D1 Mini ESP32)

### Hardware Setup (Current)

| Device | Role | Connection |
|--------|------|------------|
 | **Wemos D1 Mini ESP32** (target) | Production K-Line bridge | UART2(GPIO16 D4 / GPIO17 D3) → ISO9141 Click (L9637D) → ECU K-Line |
| **PC** | WiFi client + serial debug | COM10 (ESP32 debug 115200), WiFi `GSX-R1000_ECU` |
| **Arduino Uno** (legacy test) | K-Line bridge (retired) | AltSoftSerial D8(D9 → USB-TTL) |

### Board Specs
- ESP32-D0WD-V3 rev3.1, 240MHz, 4MB flash, WiFi + BT 4.2
- CH9102F USB-C serial, COM10, 921600 upload
- GPIO2 = built-in LED (active-LOW, shows handshake state)

### Pinout

| Signal | GPIO | Board Label | Function |
|--------|------|-------------|----------|
| K-Line RX | GPIO16 | D4 / RX2 | UART2 RX → ISO9141 Click TX |
| K-Line TX | GPIO17 | D3 / TX2 | UART2 TX → ISO9141 Click RX |
| Dealer Mode | GPIO12 | — | Input pullup (LOW=active) |
| Built-in LED | GPIO2 | D4 | Status indicator (active-LOW) |
| *(unused)* | GPIO26 | D0 | — |

**Note**: Wemos D1 Mini ESP32 D-pin mapping: D0=GPIO26, D3=GPIO17, D4=GPIO16. Not the same as ESP8266 D1 Mini.

### Features
- **WiFi AP**: `GSX-R1000_ECU`, password `suzuki2004`, IP `192.168.4.1`
- **Web dashboard**: Real-time ECU gauges, K-Line terminal log, dealer mode control
- **Bluetooth Serial**: `GSX-R1000_ECU` SPP, broadcasts JSON at 10Hz
- **LED status per handshake state**: IDLE=slow pulse, 5-baud=rapid flash, sync=double-blink, KW=quick blink, negotiate=alternating, ACTIVE=solid on
- **K-Line log**: `/api/log` endpoint, 4KB ring buffer, live terminal on dashboard

### Build & Upload
```
cd firmware/esp32_kline
python -m platformio run -e wemos_d1_mini
python -m platformio run -e wemos_d1_mini -t upload
python -m platformio device monitor -e wemos_d1_mini
```

### Baud Rate
- **ECU K-Line: 10400 baud (FIXED, never changed)**
- ESP32 debug serial: 115200 baud

### KWP2000 Handshake Protocol (ESP32 ↔ ECU)

```
ESP32 (Client)                    ECU Emulator (Server)
─────────────────                 ─────────────────────
  5-baud init 0x33 ──────────────►
  (bit-bang: 25ms low, 200ms/bit)
                                   ← 0x55 0x55 0x55 (sync)
  ← 0x08 0xF7 (KW1/KW2)
  0x81 (StartComms) ─────────────►
                                   ← 0x80 0x04 0x00 0x81 <cs>
  0x83 0x02 <cs> (AccessTiming) ─►
                                   ← 0xC3 0x02 0x00 0x00 0x00 <cs>
  ──── KWP2000 ACTIVE ────
  0x21 <PID> <cs> (ReadDataById)─►
                                   ← 0x62 <PID> <data...> (positive)
                                   ← 0x7F <PID> 0x12 (negative/unsupported)
  0x82 (StopComms) ──────────────►
  ──── return to IDLE ────
```

**Checksum**: Additive sum (`uint8_t checksum(len, data...) { return sum(each byte) & 0xFF; }`)

### State Machine

```
KL_IDLE ──(dealer mode + trigger)──► KL_FAST_INIT_5BAUD
  ▲                                      │
  │                                      ▼
  │                               KL_FAST_INIT_WAIT_SYNC  (3× 0x55)
  │                                      │
  │                                      ▼
  │                               KL_FAST_INIT_WAIT_KW1  (0x08)
  │                                      │
  │                                      ▼
  │                               KL_FAST_INIT_WAIT_KW2  (0xF7)
  │                                      │
  │                                      ▼
  │                               KL_START_COMMS_SENT  (send 0x81)
  │                                      │
  │                                      ▼
  │                               KL_START_COMMS_WAIT_RESP
  │                                      │
  │                                      ▼
  │                               KL_TIMING_SENT  (send 0x83 0x02)
  │                                      │
  │                                      ▼
  │                               KL_TIMING_WAIT_RESP
  │                                      │
  │                                      ▼
  │                               KL_ACTIVE  ←── read/write live data
  │                                      │
  │                                      ▼
  └────────────────────────────── KL_STOP_COMMS
```

### Supported PIDs (ReadDataById = 0x21)

| PID | Name | Size | Units | Decode |
|-----|------|------|-------|--------|
| 0x08 | RPM | 2 bytes | rev/min | little-endian: `b[0] \| (b[1]<<8)` |
| 0x0B | Intake Air Temp | 1 byte | °C | raw value |
| 0x0C | Gear Position | 1 byte | — | 0=N, 1-6 |
| 0x0D | Coolant Temp | 1 byte | °C | raw value |
| 0x0F | Manifold Pressure | 1 byte | kPa | raw value |
| 0x11 | Throttle Position | 1 byte | % | raw value |

**Response format**: `[0x62, pid, data...]` (positive), `[0x7F, pid, 0x12]` (negative/unsupported)

### API Endpoints

| Endpoint | Method | Description |
|----------|--------|-------------|
| `/` | GET | HTML dashboard (gauges + K-Line terminal) |
| `/api/ecu` | GET | JSON live ECU data (RPM, temps, TPS, etc.) |
| `/api/kline` | GET | K-Line status (connected, baud, dealer mode) |
| `/api/status` | GET | System status (heap, flash, WiFi mode) |
| `/api/log` | GET | K-Line terminal log (plain text, ring buffer) |
| `/api/cmd` | POST | Commands: `reset`, `start_kline`, `calibrate`, `flash` |
| `/api/dealer` | POST | Dealer mode: `enter` or `exit` |

### Test Results

#### Arduino Uno (AltSoftSerial, 10400 baud)
- ✓ Full handshake → ACTIVE verified
- ✓ ReadDataById PID requests working (pending upload)
- Known bugs fixed: double-begin, rxIdx>=3, dead else-if

#### Wemos D1 Mini ESP32
- ✓ Builds and uploads via PlatformIO (espressif32 @ 6.5.0)
- ✓ WiFi AP active at 192.168.4.1
- ✓ Bluetooth SPP active as `GSX-R1000_ECU`
- ✓ Web dashboard serves real-time gauges + terminal log
- ✓ LED status patterns for each handshake state
- ✗ Full K-Line handshake test with ECU emulator pending (stuck at 5-baud init — dealer mode activation needed)
- ✓ Dealer mode API works via `/api/dealer?action=enter` (query-param fallback)
- ✓ Dealer mode API works via `POST /api/cmd?cmd=start_kline` (query-param fallback)

### Historical Bugs (Arduino → ESP32 migration)

1. **Double `kline.begin()`**: AltSoftSerial begin called twice → silent write failure. Fixed: call once in setup().
2. **`rxIdx >= 3` → `>= 2`**: ReadDataById request is 2 bytes (`[0x21, PID]`), not 3.
3. **Dead `else if`**: Missing braces in ACTIVE handler made StopComms unreachable.
4. **Timeout in KL_IDLE (ESP32)**: `klineCheckTimeout()` firing in IDLE state. Fixed: skip timeout check when `klineState == KL_IDLE`.
5. **Bluetooth linker error (older PlatformIO)**: Fixed by updating `platform = espressif32 @ ^6.0.0`.
6. **Spurious ISO9141_EN_PIN**: No enable pin exists on the ISO9141 Click board. Removed `ISO9141_EN_PIN=4` from code, platformio.ini, and docs. The L9637D INH pin is tied to VCC (always enabled).
7. **Wrong D-pin labels on Wemos D1 Mini ESP32**: AGENTS.md and comments claimed GPIO16=D0, but actual board has D0=GPIO26, D4=GPIO16, D3=GPIO17. Fixed all pinout docs. Note: Wemos D1 Mini ESP32 D-pin layout differs from the ESP8266 version.
8. **`hasArg("plain")` unreliable for JSON POST**: ESPAsyncWebServer `request->hasArg("plain")` returned false even with valid JSON body. Fixed both `handleDealerRequest` and `handleCommandRequest` to parse body unconditionally, with query-parameter fallback (`?action=enter` / `?cmd=start_kline`).

### Files

#### Firmware
| File | Platform | Description |
|------|----------|-------------|
| `firmware/arduino_kline/arduino_kline.ino` | Arduino Uno | KWP2000 handshake (AltSoftSerial, legacy) |
| `firmware/esp32_kline/src/main.cpp` | Wemos D1 Mini ESP32 | Production K-Line bridge + WiFi AP + BT + web UI |
| `firmware/esp32_kline/platformio.ini` | PlatformIO | Build config, COM10, UART2 pins, BT enabled, no EN pin |
| `android/` | Android (Kotlin + Jetpack Compose) | BT SPP app scaffolded: dashboard gauges, K-Line terminal, controls |

#### Python Tools
| File | Purpose |
|------|---------|
| `tools/ecu_emulator_gui.py` | GUI ECU emulator with 6 analog gauges, traffic log, manual controls |
| `tools/test_read_data.py` | Sends ReadDataById PIDs after handshake, decodes responses |
| `tools/handshake_complete.py` | Full KWP2000 handshake test against Arduino bridge |
| `tools/ecu_emulator_monitor.py` | SDS-protocol ECU emulator (different framing format) |
| `tools/rx_calib.py` | RX calibration — sends 0x55 bytes, checks reception |

### Key Protocol Differences

| Aspect | Arduino/ESP32 Firmware | `ecu_emulator_monitor.py` |
|--------|----------------------|---------------------------|
| Frame format | Raw bytes (no header) | `[0x12, len, svc, data, cs]` |
| Checksum | Additive sum (`+=`) | XOR (`^=`) |
| ReadDataById | Simple `[0x21, PID]` | SDS-style local IDs with live data block |

### Next Steps

1. Connect to ESP32 WiFi, hit `http://192.168.4.1/api/dealer?action=enter` to activate dealer mode
2. ESP32 should auto-send 5-baud init within 2s → verify handshake reaches ACTIVE in emulator
3. Run ReadDataById test at 10400 baud (`python tools/test_read_data.py`)  
4. Verify against real Suzuki GSX-R1000 K4 ECU
5. Build + test Android APK (`android/` — BT SPP app scaffolded)
6. ECU flash read/write via K-Line bootloader protocol
7. DTC lookup — Suzuki-specific codes
