# ESP32-Based SDS Interface — Better MCU with BT + Display

## Why ESP32 over Arduino Nano

| Feature | Arduino Nano | ESP32 |
|---------|-------------|-------|
| CPU | 16MHz 8-bit | **240MHz dual-core 32-bit** |
| RAM | 2KB | **520KB** |
| Bluetooth | ❌ None | **✅ Classic SPP + BLE** |
| WiFi | ❌ None | **✅ 2.4GHz 802.11 b/g/n** |
| UARTs | 1 (shared with USB) | **3 independent UARTs** |
| Display driving | Painful (bit-banging) | **Native SPI, 40MHz clock** |
| Price | $3-5 | **$5-8** |
| KWP2000 support | Via `Serial` (hogs USB) | **UART2 dedicated to K-Line** |
| Future: CAN bus | Needs external controller | **Built-in CAN controller (TWAI)** |

## Hardware Architecture (All-in-One)

```
                    ┌──────────────────────────┐
                    │       ESP32 Dev Board     │
                    │                          │
  ┌─────────┐       │  ┌──────┐   ┌─────────┐  │       ┌────────────┐
  │ Bike    │ K-Line│  │L9637D│   │Bluetooth│  │─ SPP ─│ Python     │
  │ 6-pin   │───────│──│ K-Line│   │Classic  │  │       │ Dashboard  │
  │ Diag    │ DM    │  │IC    │   │SPP      │  │       │ (laptop)   │
  │ Port    │───────│──│4N35  │   └─────────┘  │       └────────────┘
  │         │ +12V  │  │Opto  │   ┌─────────┐  │ 
  │         │───────│──│LM7805│   │  WiFi   │  │──AP──┐ Smartphone
  │         │ GND   │  │Reg   │   │  AP+Web │  │      │ Web Browser
  └─────────┘       │  └──────┘   └─────────┘  │       └────────────┘
                    │                          │       
                    │  ┌──────────────────┐    │       ┌────────────┐
                    │  │  TFT Display     │    │       │ SD Card    │
                    │  │  ILI9341 320x240 │────│───────│ Data Log   │
                    │  │  (SPI)          │    │       └────────────┘
                    │  └──────────────────┘    │
                    └──────────────────────────┘
```

## Bluetooth Modes

### Mode 1: Bluetooth SPP (Wireless Serial — Zero Config)
- ESP32 appears as "SDS-Reader" for pairing (PIN: 1234)
- Streams all sensor data as CSV at 10Hz
- Python dashboard connects via `bluetooth-serial` library
- No wiring changes from Arduino build

### Mode 2: WiFi Access Point (Phone Access — No App)
- ESP32 creates WiFi AP named "SDS-Reader"
- Connect phone to it, open browser to `192.168.4.1`
- Full web dashboard with real-time gauges
- Works on any device with a browser

### Mode 3: TFT Display (Standalone — No PC/Phone)
- On-device gauge cluster with:
  - Analog RPM gauge (needle sweep)
  - Digital speed + gear readout
  - Coolant temp + TPS bars
  - Battery voltage
  - Injector pulse monitor
- Touch screen for mode switching

## Updated Pin Assignments (ESP32)

| ESP32 GPIO | Connected To |
|------------|-------------|
| GPIO16 (RX2) | L9637D pin 2 (RX from K-Line) |
| GPIO17 (TX2) | L9637D pin 1 (TX to K-Line) |
| GPIO4 | 4N35 anode (via 330Ω) — dealer mode |
| GPIO5 | TFT_CS |
| GPIO18 | TFT_CLK (SPI SCK) |
| GPIO19 | TFT_MOSI |
| GPIO23 | TFT_DC |
| GPIO2 | TFT_RST |
| GPIO21 | TFT backlight (PWM capable) |
| GPIO13-15 | SD Card (SPI) — optional |
| GPIO0 | Boot button (mode select) |

## Full Component List

| Qty | Part | Purpose | Cost |
|-----|------|---------|------|
| 1 | **ESP32 Dev Board** (30-pin) | Main MCU | $5 |
| 1 | L9637D | K-Line transceiver | $2 |
| 1 | 4N35 | Dealer mode optocoupler | $0.50 |
| 1 | LM7805 | 12V→5V for ESP32 | $0.50 |
| 1 | **ILI9341 2.8" TFT** | Gauge cluster display | $12 |
| 1 | SD Card module | Data logging | $2 |
| 1 | 1kΩ, 330Ω, 10kΩ resistors | Pull-ups + opto | $0.30 |
| 1 | 6-pin Sumitomo harness | Bike connector | $5-50 |
| **Total** | | | **~£25-60** |

## UK Sourcing Summary

| Part | UK Supplier | Approx £ |
|------|-------------|----------|
| ESP32 Dev Board | The Pi Hut / Pimoroni | £6 |
| L9637D | Farnell (element14) | £3.50 |
| 4N35 | Farnell | £0.70 |
| LM7805 | Farnell / RS | £0.50 |
| ILI9341 TFT | The Pi Hut / Amazon UK | £12 |
| FTECU harness | ftecucanada.com (ships UK) | £45 |
| Resistors/caps | eBay UK / Amazon UK | £2 |
| **Total without harness** | | **~£25** |
| **Total with FTECU harness** | | **~£70** |

**UK stores to bookmark:**
- **The Pi Hut** (pihut.com) — ESP32, TFTs, breadboards, all maker gear
- **Pimoroni** (pimoroni.com) — Same, excellent selection
- **Farnell / element14** (uk.farnell.com) — ICs, proper components, next-day delivery
- **RS Components** (uk.rs-online.com) — Industrial supply, everything
- **Cool Components** (coolcomponents.co.uk) — Arduino/ESP32 specialist
- **Amazon UK** (amazon.co.uk) — Fast delivery for dev boards and displays
