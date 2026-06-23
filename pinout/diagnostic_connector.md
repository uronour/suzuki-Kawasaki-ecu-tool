# 2004 Suzuki GSX-R1000 Diagnostic Connector Pinout

## Overview

Your 2004 GSX-R1000 (K3/K4) uses a **6-pin Sumitomo diagnostic connector** located under the front seat, near the ECU. This is the same connector system used on all fuel-injected Suzuki sportbikes, but **the pinout changed between model years**.

**FTECU bench harness types confirm:**
| Model Year | Bench Harness Type |
|---|---|
| 2003-2004 GSXR1000 (K3/K4) | **Type 11** (shared with 02-07 Hayabusa) |
| 2005-2006 GSXR1000 (K5/K6) | **Type 4** |

This confirms the pinout is **different** from the 2005+ models documented in the AIM SDS protocol PDF.

## 6-Pin Diagnostic Connector

The connector is a Sumitomo 6-pin housing, similar to Honda's 4-pin but with 6 positions.

### Connector View (Looking at the bike-side harness receptacle)

```
    ___________
   /           \
  |  1       6  |
  |  2       5  |
  |  3       4  |
   \___________/
```

### Verified Pinout for 2003-2004 GSXR1000 (Type 11 / Type 5)

| Pin | Wire Color | Function | Notes |
|-----|-----------|----------|-------|
| 1 | Br/W (Brown/White) | K-Line | ISO 9141 / KWP2000 data line, 10400 baud |
| 2 | R (Red) | +12V always-on | Battery voltage, powers interface |
| 3 | B/W (Black/White) | Ground | Chassis ground |
| 4 | O (Orange) | **DM (Dealer Mode)** | Pull LOW to enter dealer mode. Requires optocoupler. |
| 5 | -- | **Mode Select Switch** | Connected to mode select switch coupler on 03-04 models |
| 6 | -- | Not connected | Reserved |

### Critical Differences: 2003-2004 vs 2005+

The 2003-2004 models have a **Mode Select Switch coupler** (separate from the diagnostic connector). On 2005+ models, the dealer mode function was integrated into the 6-pin connector (Pin 4). On the 2003-2004, you may need to:

1. Use the mode select switch coupler (special tool 09930-82720) **OR**
2. Connect directly to the ECU pin for dealer mode

### Mode Select Switch Coupler

The 2003-2004 has a separate 4-pin mode select switch coupler (white, near the ECU):
- Connect the special tool (09930-82720 - a simple on/off switch) to this coupler
- Turning the switch ON puts the ECU in dealer mode
- This allows DTC reading and advanced diagnostic functions

**In our custom interface**, you can replace the physical switch with an optocoupler controlled by the Arduino, which simulates this switch closure.

## ECU Connector Pinout (Direct Connection Option)

If you cannot find or access the 6-pin diagnostic connector, you can connect directly to the ECU.

The 2003-2004 GSXR1000 ECU (Denso, part #32920-18G20) has **two large connectors**: C37 (60-pin) and E23 (34-pin).

### K-Line Access via ECU

The K-Line is available on **ECU connector C37** (the larger of the two connectors):

| ECU Pin | Wire Color | Function |
|---------|-----------|----------|
| C37-?? | Br/W | K-Line (SDS data) |
| C37-?? | R/Bl | +12V main power |
| C37-?? | B/W | Ground |

**Full ECM pinout reference**: Download the xlsx file from gixxer.com forums:
https://www.mediafire.com/file/vaicfqjd3nov5f6/ECM_pinouts.xlsx

## Verification with Multimeter

Before connecting your interface, verify the connector pins:

1. **Pin 1 (K-Line)**: With ignition ON, should read ~11-12V (pulled high by ECU)
2. **Pin 2 (+12V)**: Always ~12V (battery voltage)
3. **Pin 3 (GND)**: 0V / continuity to battery negative
4. **Pin 4 (DM)**: ~5V when NOT in dealer mode, 0V when activated
5. **Pin 5**: Measure voltage with ignition ON - this tells us if it's the mode select line

## Shopping Options

### Option A: Buy the FTECU Harness (Guaranteed to Fit)

| Item | Part | Cost |
|------|------|------|
| Bike-side harness | FTECU 6-Pin Bike Side Harness Type 5 | $49.95 |
| Bench harness (optional) | FTECU 6-Pin Bench Harness Type 11 | $99.95 |

Buy at: https://ftecu.com/shop/
Or: https://2wheeldynoworks.com/

These harnesses give you the correct mating connector with pigtails, so you don't need to source the Sumitomo connector separately.

### Option B: Build Your Own Cable

- Sumitomo 6-pin connector housing: Search "Sumitomo 6-pin motorcycle diagnostic connector" on AliExpress (~$5)
- Or use individual female spade terminals if you can't find the connector
- Wire: 18-22 AWG automotive grade
