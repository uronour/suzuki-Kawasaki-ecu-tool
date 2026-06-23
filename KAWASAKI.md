# Kawasaki KDS Protocol Support

Same hardware, different protocol configuration.

## Differences from Suzuki SDS

| Parameter | Suzuki SDS | Kawasaki KDS |
|-----------|-----------|--------------|
| ECU Address | 0x12 | 0x11 |
| Protocol Base | KWP2000 (ISO 14230) | KWP2000 (ISO 14230) |
| Baud Rate | 10,400 | 10,400 |
| Init Sequence | fast init (0x81) | fast init (0x81) + diagnostic session (0x10 0x80) |
| Physical Layer | K-Line (ISO 9141-2) | K-Line (ISO 9141-2) |
| Hardware | Identical | Identical |

## KDS Specific PIDs

The KWP2000 library has these KDS-specific PIDs:
- `kawasaki_request_gps` — Gear position (0x21, 0x0B)
- `kawasaki_request_rpm` — RPM (0x21, 0x09)
- `kawasaki_request_speed` — Speed (0x21, 0x0C)
- `kawasaki_request_tps` — Throttle position (0x21, 0x04)
- `kawasaki_request_iap` — Intake pressure (0x21, 0x07)
- `kawasaki_request_iat` — Intake temp (0x21, 0x05)
- `kawasaki_request_ect` — Coolant temp (0x21, 0x06)

## Kawasaki Diagnostic Connector

Most Kawasaki fuel-injected models (2004+) use a **6-pin connector** similar to Suzuki but with different pinout.

Typical Kawasaki 6-pin diagnostic connector:
```
Pin 1: K-Line
Pin 2: +12V
Pin 3: Ground  
Pin 4: Not used
Pin 5: Not used
Pin 6: Not used
```

## Using with This Hardware

1. Build the same hardware (Arduino + L9637D)
2. In firmware, change `brand` to `KAWASAKI`
3. Note: Kawasaki doesn't need the dealer mode optocoupler
4. The KWP2000 library handles all KDS protocol differences automatically

## Tested Kawasaki Models

The library has been tested on:
- Kawasaki ZX-6R (636)
- Various Kawasaki models 2005-2015

(Verify your specific model on the KWP2000 GitHub issues page.)
