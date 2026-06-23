# Bill of Materials — Suzuki SDS K-Line Interface

## Core Components

| Qty | Part | Description | UK Source | Cost (GBP) |
|-----|------|-------------|-----------|------------|
| 1 | **ESP32 Dev Board** (30-pin) | Main MCU, WiFi+BT built-in | The Pi Hut / Pimoroni / Amazon UK / eBay UK | £4-8 |
| 1 | L9637D | K-Line ISO 9141 transceiver IC | Farnell (element14) / RS Components / eBay UK | £2-4 |
| 1 | 4N35 | Optocoupler for dealer mode | Farnell / RS / eBay UK | £0.50 |
| 1 | LM7805 | 12V to 5V voltage regulator | Farnell / RS / eBay UK / Maplin-style shops | £0.50 |
| 1 | ILI9341 2.8" TFT (optional) | Colour gauge cluster display | The Pi Hut / Pimoroni / Amazon UK / eBay UK | £10-15 |
| 1 | Mini breadboard + wires | Prototyping | The Pi Hut / Pimoroni / Amazon UK / eBay UK | £3 |
| 1 | 6-pin Sumitomo connector | Diagnostic connector | **See note below** | £5-45 |

## Resistors & Capacitors

Any UK electronics shop (eBay UK, Amazon UK, RS, Farnell, local shop):

| Qty | Value | Purpose |
|-----|-------|---------|
| 1 | 1kΩ 1/4W | K-Line pull-up to +12V |
| 1 | 330Ω 1/4W | Optocoupler LED current limit |
| 1 | 10kΩ 1/4W | Optocoupler output pull-up |
| 2 | 100nF ceramic | Decoupling caps for L9637D/7805 |
| 1 | 10µF electrolytic | Output cap for 7805 |

## Optional Components

| Qty | Part | Description | UK Source | Cost |
|-----|------|-------------|-----------|------|
| 1 | SD Card module | Data logging | The Pi Hut / Pimoroni / eBay UK | £2 |
| 1 | USB-TTL adapter | FTDI FT232RL debugging | The Pi Hut / Pimoroni / Amazon UK | £4 |
| 1 | Logic analyzer | Saleae clone 8ch | eBay UK | £8 |
| 1 | Project box | Enclosure | The Pi Hut / Pimoroni / Amazon UK | £3 |

## Estimated Total

- **Minimum build** (ESP32 + L9637D + basic parts): **~£10-15**
- **With TFT display** (standalone gauge cluster): **~£25-30**
- **With FTECU harness** (guaranteed fit, no soldering on bike connector): **~£55-70**

## UK Sourcing Guide

### ESP32
| Store | URL | Price |
|-------|-----|-------|
| **The Pi Hut** | www.pihut.com | ~£6 |
| **Pimoroni** | www.pimoroni.com | ~£6 |
| **Amazon UK** | www.amazon.co.uk | £5-8 |
| **eBay UK** | www.ebay.co.uk | £4-6 |
| **Cool Components** | www.coolcomponents.co.uk | ~£7 |

### L9637D (K-Line IC)
| Store | URL | Price |
|-------|-----|-------|
| **Farnell (element14)** | uk.farnell.com (search 2842898) | ~£3.50 |
| **RS Components** | uk.rs-online.com (search 5344111) | ~£5 |
| **eBay UK** | search "L9637D" | ~£2 |

### 4N35 (Optocoupler)
| Store | URL | Price |
|-------|-----|-------|
| **Farnell** | uk.farnell.com (search 1085088) | ~£0.70 |
| **RS Components** | uk.rs-online.com | ~£0.80 |
| **eBay UK** | search "4N35" | ~£0.30 |

### ILI9341 TFT Display (2.8" or 3.2")
| Store | URL | Price |
|-------|-----|-------|
| **The Pi Hut** | www.pihut.com (search "ILI9341") | ~£14 |
| **Pimoroni** | www.pimoroni.com (search "display") | ~£15 |
| **Amazon UK** | www.amazon.co.uk (search "2.8 TFT SPI") | £10-14 |
| **eBay UK** | search "ILI9341 2.8 inch" | £8-12 |

### The 6-Pin Diagnostic Connector

**This is the hardest part to source in the UK.** Options:

| Option | Source | Cost | Notes |
|--------|--------|------|-------|
| **FTECU bike-side harness** | www.ftecucanada.com (ships to UK) | ~£45 | Guaranteed fit, easiest option |
| **eBay UK** | search "Suzuki 6 pin diagnostic connector motorcycle" | ~£10-15 | Unknown fit, may need modification |
| **AliExpress** | search "Sumitomo 6 pin motorcycle connector" | ~£5 | Ships from China, 2-4 weeks |
| **Build from ECU pins** | Use individual female spade terminals | ~£2 | Avoids needing the connector entirely |

**Recommendation:** Start with FTECU harness if budget allows, or build from ECU pins with spade terminals for testing. Once you've verified the pinout, you can add a proper connector later.

### General Electronics
| Store | URL | Best For |
|-------|-----|----------|
| **The Pi Hut** | www.pihut.com | ESP32, displays, breadboards |
| **Pimoroni** | www.pimoroni.com | ESP32, displays, breakout boards |
| **Farnell (element14)** | uk.farnell.com | ICs, resistors, proper components |
| **RS Components** | uk.rs-online.com | ICs, tools, components |
| **Cool Components** | www.coolcomponents.co.uk | Arduino, ESP32, sensors |
| **Amazon UK** | www.amazon.co.uk | Everything, fast delivery |
| **eBay UK** | www.ebay.co.uk | Cheapest, variable quality |
