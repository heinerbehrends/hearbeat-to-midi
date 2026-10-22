# 003 – External WS2812D status LED on GPIO21

**Status:** todo
**Depends on:** –

## Acceptance criteria

- [ ] [hw] Status LED is driven on **GPIO21**; GPIO48 (onboard, disconnected on this clone by the open
      RGB solder jumper) is no longer used.
- [ ] [hw] Colours match the states (existing logic in `loop()`):
      - **red blinking** (2 Hz, 250 ms on / 250 ms off) = NoSensor (unplug the sensor to check)
      - **red steady** = NoFinger
      - **amber** = Acquiring, or Locked but the last reading was missing/rejected
      - **green dim, bright flash (80 ms) on every clock beat** = Locked and last reading accepted
- [ ] [hw] Red and green are not swapped (WS2812D expects GRB); if they are, fix with
      `rgbLedWriteOrdered(..., LED_COLOR_ORDER_GRB, ...)` / the right order constant, not by swapping r/g
      in the state logic.
- [ ] [hw] Brightness is low by default: steady states at about 1/16 of full scale (`LED_LEVEL` ≈ 16),
      dim green ≈ 1/8 of that. Comfortable to look at from 50 cm in a dim room; one constant to change.
- [ ] [hw] Beat flash stays in time with the DAW's beat indicator (no visible lag or jitter).
- [ ] [hw] LED stays dark/stable during boot and USB enumeration (no random colour burst lasting > 1 s).
- [ ] [log] No change to the log.

## Context

The LED logic exists but drives GPIO48, which is not connected on this board. An external 8 mm LED in
the enclosure replaces it.

## Notes / Design

- Change `LED_PIN` to 21 and `LED_LEVEL` in `src/main.cpp`; `rgbLedWrite()` (RMT based, core 3.x) works
  on any output GPIO.
- `loop()` only writes when the colour changes – keep that, the RMT write is not free.
- GPIO21 is not a strapping pin and not used by USB (19/20) or the sensor (17/9).

## Hardware

WS2812D-F8 (8 mm through-hole, addressable RGB). Pinout of the F8: check the datasheet of the actual
part – usually DIN, VDD, GND, DOUT with the longest leg = VDD or GND depending on vendor.

```
5V (USB VBUS) ──|>|── VDD (WS2812D)        1N4148, anode at 5V → VDD ≈ 4.3 V
                 │
               100 nF                      ceramic, right at the LED pins
                 │
GND ──────────── GND (WS2812D)

GPIO21 ──[330 Ω]── DIN                     resistor close to the LED
                    DOUT                   unconnected
```

- Why the diode: at 5 V the WS2812 needs DIN ≥ 0.7 × VDD = 3.5 V, more than the 3.3 V GPIO gives.
  Dropping VDD to ~4.3 V makes the threshold ≈ 3.0 V, so 3.3 V data is valid without a level shifter.
- 330 Ω damps ringing on the data line and limits current into DIN if the LED is powered while the ESP is
  not. 1N4148 is fine for one LED (max ~60 mA at full white, far less at our brightness).
- Common GND between LED and ESP32-S3 board.

## Out of scope

- More than one LED / a chain.
- Showing switch positions or the valid-reading rate on the LED.
- Fixing the onboard RGB jumper.
