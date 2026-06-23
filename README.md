# Suzuki SDS ECU Tool — 2004 GSX-R1000

> **Status: ONGOING — awaiting parts (L9637D, HC-05) for K-Line and BT testing**
>
> Custom diagnostic tool for the Suzuki SDS ECU using a BigTreeTech TFT35 V3.0 as a standalone display/controller, with KWP2000 over K-Line and Bluetooth SPP streaming.

![TFT35 V3.0 running custom gauge firmware](docs/images/tft35_display.jpg)

## Overview

This project replaces the Suzuki factory dealer tool with a custom-built diagnostic unit. It reads real-time sensor data (RPM, speed, temperatures, TPS, battery voltage, gear position, etc.) from the ECU over the K-Line and displays it on a 3.5" colour TFT with an analog tachometer gauge.

**Target ECU:** Denso 32920-18G20 (2004 GSX-R1000 SDS)
**Protocol:** KWP2000 (ISO 14230) over K-Line (ISO 9141-2), 10400 baud 8N1
**Controller:** STM32F207VCT6 (on TFT35 V3.0)

## Hardware

### TFT35 V3.0 (BigTreeTech)
- STM32F207VCT6 Cortex-M3, 256KB flash, 128KB SRAM
- 3.5" 480x320 ILI9488 TFT with XPT2046 touch
- USART1 (WiFi port → HC-05 BT module)
- USART2 (RS232 port)
- USART3 (DIY port → K-Line via L9637D)
- Rotary encoder (PA8/PC9/PC8) for physical navigation
- SD card slot for data logging and firmware updates

### K-Line Interface (PCB Board A)
- L9637D K-Line transceiver
- TVS protection on K-Line input
- 5V→3.3V level shifting for STM32 UART
- 3-pin diagnostic input header (GND, K-Line, +12V)
- 4-pin UART output header (common pinout)

### Bluetooth Module (PCB Board B)
- HC-05 BT module socket
- Same 4-pin UART header as Board A for modular swap
- Connects to TFT35 WiFi port (USART1)

Both PCBs use 2.54mm pitch headers with a common pinout so BT and K-Line can be swapped without soldering.

## Firmware

### `firmware/btt-custom/` — TFT35 Custom Firmware

A stripped-down firmware based on the BTT TFT35 source, rewritten for SDS diagnostics.

**What was removed:**
- All printer G-code parsing and menu system (50+ Menu/ directories)
- Language/localisation files
- Vfs API (virtual filesystem)
- Non-F2xx HAL directories (F1, F4, GD32 variants)
- Unused LCD drivers (SSD1963, HX8558, RM68042, ILI9341, ILI9325, ST7789, ST7796S)
- Buzzer, knob LED, LCD encoder, W25QXX flash driver
- ST7796S removed from LCD driver mask (requires deleted BTT GUI_COLOR type)

**Custom modules written:**
| Module | Description |
|--------|-------------|
| `SDSProtocol.c/h` | KWP2000 protocol engine — start/stop communication, sensor polling, keep-alive, DTC read/clear, dealer mode |
| `kline_task.c/h` | K-Line hardware driver — USART3, fast init wakeup (1000ms HIGH → 25ms LOW → 25ms HIGH), byte-level TX/RX |
| `bt_stream.c/h` | Bluetooth SPP streaming via USART1 (WiFi port) — JSON sensor data output |
| `gauge_ui.c/h` | Main gauge UI — dark grey carbon-fibre background, analog tachometer (270° sweep, green/yellow/red zones), 4 digital readouts, smart redraw (no flicker) |
| `gauge_dial.c/h` | Analog gauge dial drawing — arc sweep, ticks, needle (Bresenham line), centre hub, lookup-table sin/cos |
| `ili9488_gfx.c/h` | LCD graphics primitives — scaled font rendering (GFX_DrawCharScaled, GFX_DrawStringScaled), coloured arc drawing, backlight control |
| `encoder.c/h` | Polled quadrature rotary encoder driver with debounce — rotate for page nav, press placeholder for detail mode |
| `font_8x13.h` | 8x13 monospace font glyph table |

**Key integration points:**
- `OS_InitTimerMs()` must be called before any `Gauge_Update()` — timer starts the display refresh cycle
- `RCC_GetClocksFreq()` must be initialised before `Delay_init()` for correct timing
- LCD auto-detects ILI9488 or NT35310 at runtime via `LCD_Init()`
- Touch: left half → prev page, right half → next page
- Encoder: rotate CW/CCW changes pages, press advances page

**Flash layout:**
```
0x08000000 - 0x08003FFF  Bootloader (stock BTT, 16KB)
0x08004000 - 0x08007FFF  Config (16KB)
0x08008000 - 0x0803FFFF  Application (224KB)
```

**Build:** PlatformIO with custom board `STM32F207VC_0x8000`, CMSIS framework, OpenOCD upload via ST-Link V2.

### `firmware/sds-reader/` — Arduino Sketches

| Sketch | Target | Purpose |
|--------|--------|---------|
| `sds-reader.ino` | Arduino Uno/Mega | Main reader using aster94 KWP2000 library |
| `sds_standalone.ino` | Arduino Uno/Mega | Standalone version (no external lib deps) |
| `sds_esp32_web.ino` | ESP32 | Web dashboard over WiFi |
| `sds_esp32_tft.ino` | ESP32 + TFT | Standalone display unit |
| `sds_esp32_bt.ino` | ESP32 | Bluetooth serial output |
| `ecu_simulator.ino` | Arduino Uno/Mega | Simulates ECU on bench (for testing without bike) |
| `kline_sniffer.ino` | Arduino Uno/Mega | Passive K-Line traffic monitor |
| `SDSProtocol.cpp/h` | Arduino | Standalone KWP2000 protocol library |

### `python/` — PC Tools

**sds_dashboard.py** — Real-time Tkinter dashboard with gauges, live graphs, CSV logging, DTC reading.

## Protocol Details (SDS / KWP2000)

### Fast Init Sequence
```
Tester:  ──────── HIGH 1000ms ──── LOW 25ms ──── HIGH 25ms ────┐
                                                                │
Tester:  ── startCommunication(0x81) ──────────────────────────→│
ECU:     ── 0xC1, keybyte1, keybyte2 ──────────────────────────→│
Tester:  ── readTimingParameters(0x83, 0x02) ──────────────────→│
ECU:     ── timing parameters (P2, P3, P4) ────────────────────→│
```

### Sensor Polling
```
Request:  {0x82, 0x12, 0xF1, 0x21, 0x08, CS}
          format=0x82(0x80|2), target=0x12, source=0xF1,
          pid=ReadDataByLocalIdentifier(0x21) with ID=0x08

Response: {0x80, 0xF1, 0x12, 0x34, 0x61, 0x08, sensor_data(52B), CS}
          format=0x80, length=0x34(52), response=0x61(0x21|0x40)
```

### Sensor Data Layout (response byte indices)

| Index | Value | Formula |
|-------|-------|---------|
| 16 | Speed | raw × 2 (km/h) |
| 17-18 | RPM | byte[17] × 10 + byte[18] / 10 |
| 19 | TPS | 125 × (raw − 55) / (256 − 55) % |
| 20 | MAP | raw × 4 × 0.136 kPa |
| 21 | Coolant Temp | (raw − 48) / 1.6 °C |
| 22 | Intake Air Temp | (raw − 48) / 1.6 °C |
| 24 | Battery Voltage | raw × 100 / 126 V |
| 26 | Gear Position | raw (0 = neutral) |
| 46 | STPS | raw / 2.55 % |
| 52 | Clutch | raw != 0 |

### Keep-Alive
```
{0x3E, 0x01} — testerPresent with response, sent every ~900ms
```

## Project Structure

```
suzuki-ecu-tool/
├── firmware/
│   ├── btt-custom/              ← TFT35 V3.0 custom firmware (primary)
│   │   ├── platformio.ini
│   │   ├── buildroot/           # Board definitions, scripts, linker
│   │   └── src/
│   │       ├── User/            # Custom application code
│   │       │   ├── main.c
│   │       │   ├── SDSProtocol.c/h
│   │       │   ├── kline_task.c/h
│   │       │   ├── bt_stream.c/h
│   │       │   ├── gauge_ui.c/h
│   │       │   ├── gauge_dial.c/h
│   │       │   ├── ili9488_gfx.c/h
│   │       │   ├── encoder.c/h
│   │       │   └── font_8x13.h
│   │       ├── Hal/             # LCD, touch, UART drivers
│   │       ├── Variants/        # Pin definitions
│   │       └── Libraries/       # CMSIS + FWLib
│   │
│   └── sds-reader/              # Arduino sketches (secondary)
│       ├── sds-reader.ino
│       ├── sds_standalone.ino
│       ├── sds_esp32_*.ino
│       ├── ecu_simulator.ino
│       ├── kline_sniffer.ino
│       └── SDSProtocol.cpp/h
│
├── hardware/
│   ├── BOM.md                   # Parts list
│   ├── wiring.md                # Wiring instructions
│   ├── sds-interface.pro        # KiCad PCB project
│   └── esp32_upgrade.md         # ESP32 OTA notes
│
├── pinout/
│   └── diagnostic_connector.md  # Suzuki 6-pin diagnostic connector
│
├── python/
│   ├── sds_dashboard.py         # PC live dashboard
│   └── requirements.txt
│
├── docs/
│   └── images/                  # Photos, screenshots
│
├── KAWASAKI.md                  # KDS protocol notes
├── README.md
└── .gitignore
```

## Current Status

### ✅ Done
- Protocol research and KWP2000 library analysis (aster94 KWP2000)
- BOM, wiring diagram, KiCad schematic
- TFT35 V3.0 firmware with stripped BTT codebase
- Custom gauge UI with analog tachometer, digital readouts, dark theme
- Rotary encoder and touch navigation
- LCD auto-detection (ILI9488 / NT35310)
- XPT2046 touch driver enabled
- Scaled font rendering for large readouts
- Speed unit conversion (km/h → mph)
- K-Line fast init and protocol engine
- Bluetooth streaming (USART1 → HC-05)
- Flash dump and backup preserved
- Builds at ~24KB (224KB available)

### 🔄 In Progress
- Custom PCB design — Board A (L9637D K-Line) + Board B (HC-05 socket), with common 4-pin UART header

### ⏸️ Blocked (awaiting parts)
- L9637D K-Line transceiver ICs
- HC-05 Bluetooth modules
- Passives (resistors, caps, TVS)
- PCB fabrication/verification
- End-to-end K-Line + BT testing with real ECU
- Breadboard prototype testing
- Encoder press → detail/info mode
- SD card data logging

## Getting Started (for development)

### Prerequisites
- ST-Link V2 programmer
- PlatformIO (or Arduino IDE for sds-reader sketches)
- TFT35 V3.0 board
- STM32F207VCT6 toolchain (included with PlatformIO ststm32 platform)

### Building the TFT35 Firmware
```bash
cd firmware/btt-custom
platformio run
```

### Uploading via ST-Link
```bash
platformio run --target upload
```

### Building Arduino Sketches
```bash
cd firmware/sds-reader
pio run -t upload
```

## Parts Sourcing (UK)

See `hardware/BOM.md` for full parts list with UK suppliers (Farnell, RS, The Pi Hut, eBay UK).

## License

GPL-3.0 — see LICENSE file.

## Acknowledgements

- [aster94/KWP2000](https://github.com/aster94/Keyword-Protocol-2000) — KWP2000 Arduino library for Suzuki/Kawasaki/Yamaha/Honda
- [BigTreeTech](https://github.com/bigtreetech) — TFT35 firmware source
- [ECU Hacking forum](https://ecuhacking.activeboard.com) — SDS/KDS protocol research
