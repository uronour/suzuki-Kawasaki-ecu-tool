# Wiring Diagram — Suzuki SDS K-Line Interface

## System Architecture

```
 Bike 12V ──────┬─────── LM7805 ──────┬────── +5V rail
                 │                      │
                R1(1k)                 ├──── L9637D VCC (pin 7)
                 │                      │
    Bike K-Line ─┼─────── L9637D K-I/O └──── Arduino 5V pin
                 │         (pin 4)      
               1kΩ             │
                └──────────── L9637D TX (pin 2) ──── Arduino TX1
                              L9637D RX (pin 3) ──── Arduino RX1
                              L9637D GND (pin 1/8) ─ GND rail
                              L9637D VS  (pin 5) ─── Bike 12V

    Bike GND ──────────────── GND rail

    Bike DM ──── 4N35 pin 1 (+) ──── 330Ω ──── Arduino D4
    (Dealer)    4N35 pin 2 (-) ──── GND
                4N35 pin 4 (C) ──── +5V (via 10k pull-up)
                4N35 pin 5 (E) ──── GND
```

## Detailed Connections

### 1. L9637D K-Line Transceiver (SOIC-8 or DIP-8)

```
L9637D Pin Layout (top view):
    
    TX  ── 1 ────┬─── 8 ── GND
    RX  ── 2 ────┤  7 ── VCC (+5V)
    LO  ── 3 ────┤  6 ── NC
    K-I/O ─ 4 ────┴─── 5 ── VS (+12V)
```

| L9637D Pin | Connect To |
|------------|------------|
| 1 (TX) | MCU serial TX pin (Arduino D1/Serial1 TX on Mega) |
| 2 (RX) | MCU serial RX pin (Arduino D0/Serial1 RX on Mega) |
| 3 (LO) | Leave floating (normal)/ Connect to MCU for LIN |
| 4 (K-I/O) | Bike K-Line + 1kΩ pull-up to +12V |
| 5 (VS) | +12V (from bike battery or regulated 12V) |
| 6 (NC) | Not connected |
| 7 (VCC) | +5V regulated |
| 8 (GND) | Ground |

### 2. Voltage Regulator (LM7805)

```
LM7805 (top view):
    
    IN  GND  OUT
    ─┬──┬──┬─
     1  2  3
```

| Pin | Connect To |
|-----|------------|
| 1 (IN) | Bike +12V (via 10µF capacitor to GND) |
| 2 (GND) | Bike GND |
| 3 (OUT) | +5V rail (via 100nF capacitor to GND) |

### 3. Dealer Mode Optocoupler (4N35)

```
4N35 (DIP-6, top view):
    
      1 ───┬─── 6
      2 ───┤  5
      3 ───┴─── 4

Pin 1: Anode ── 330Ω ── Arduino D4
Pin 2: Cathode ── GND
Pin 3: NC
Pin 4: Emitter ── GND
Pin 5: Collector ── 10kΩ ── +5V AND Bike DM line
Pin 6: Base ── NC
```

### 4. Arduino Connections

| Arduino Pin | Connects To |
|-------------|-------------|
| D0 (RX) | L9637D pin 2 (RX) |
| D1 (TX) | L9637D pin 1 (TX) |
| D4 | 330Ω ── 4N35 Anode (dealer mode control) |
| 5V | L9637D VCC (pin 7), 4N35 pull-up |
| GND | Common ground |
| Vin / RAW | Leave unconnected (interface is bike-powered) |

**For ESP32:**
| ESP32 Pin | Connects To |
|-----------|-------------|
| GPIO16 (RX2) | L9637D pin 2 (RX) |
| GPIO17 (TX2) | L9637D pin 1 (TX) |
| GPIO4 | 330Ω ── 4N35 Anode |
| 5V | L9637D VCC |
| GND | Common ground |

## Power Supply Notes

- The LM7805 can supply up to 1A, which is plenty
- The L9637D can be powered from the bike's 12V directly (VS pin is rated to 36V)
- The Arduino can be powered from the regulated 5V via its 5V pin (bypass the onboard regulator)
- **Total current draw**: ~100mA (Arduino + L9637D + opto)

## Input Protection (Recommended for real use)

```
Bike 12V ──┬── 1A fuse ──┬── LM7805 IN
            │              │
            └── 1N4007 ───┘ (reverse polarity protection)

K-Line ── 100Ω ── 5.1V Zener ── GND (overvoltage protection)
```
