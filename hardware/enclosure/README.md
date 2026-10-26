# Enclosure (Hammond 1590A)

The build uses a **Seeed XIAO ESP32-S3 Plus** (21 × 17.8 mm). The ESP32-S3 DevKit clone stays the
breadboard/development board. Both are built from the same code (PlatformIO environments
`xiao-s3-plus` and `esp32-s3-n16r8`; pins per board in `include/Settings.h`).

## Layout

```
 end wall A                       top face                          end wall B
 ┌──────────────────────────────────────────────────────────────────────────┐
 │ XIAO USB-C │  ═╪═ feedback   ═╪═ rate   ═╪═ transport   (LED)  │ sensor  │
 │ (to laptop)│                                                   │ USB-C   │
 └──────────────────────────────────────────────────────────────────────────┘
   XIAO in its holder,                                    4-pin panel socket
   21 mm deep                                             16 mm deep
```

- **XIAO** in [`xiao_holder.scad`](xiao_holder.scad), glued inside end wall A, its own USB-C port
  behind a plug-sized opening (13.5 × 7.5 mm, centre ≈ 6.8 mm above the rim: the model echoes the exact
  height). No laptop panel socket needed; the XIAO has the CC resistors, so C-to-C cables work.
- **Sensor socket** (4-pin USB-C, V/D−/D+/G, no CC) in end wall B, raised above the lid lip.
- **Switches and both LEDs** on the top face, in the middle: keep the switches ~25 mm from wall A
  (XIAO + holder) and ~17 mm from wall B (socket).
- Dropping the rate toggle (rate via the settings page / CC 22 only) is under consideration
  ([ticket 018](../../tickets/018-drop-rate-toggle.md)); it would free space on the top face.

## XIAO holder

- Shelf under the board (pocket locates it, tape holds it), end stop at the far end, glue tab down
  the end wall into the 1.8 mm gap in front of the lid lip; the holder clears the lip and the corner
  bosses (22.5 mm free between them).
- Measured: PCB 1.0 mm, USB-C receptacle 2.0 mm past the PCB edge, sitting directly on the PCB
  (centre ≈ 1.6 mm above it). That puts the wall opening centre at ≈ 6.8 mm above the rim.
- Check your USB-C cable's plug moulding fits the opening (`plug_w` × `plug_h`); the moulding has to
  enter the wall for the plug to seat.
- Glue it with the board in place and a cable plugged in through the wall, so it aligns itself.

## Wiring (XIAO, front pins)

| Pin | To |
| --- | --- |
| D4 (GPIO5) | sensor socket D+ (SDA), + 4.7 kΩ to 3V3 |
| D5 (GPIO6) | sensor socket D− (SCL), + 4.7 kΩ to 3V3 |
| 3V3 / GND | sensor socket V / G, switch commons (GND) |
| D0 / D1 | feedback switch − / + |
| D8 / D9 | rate switch slow / fast |
| D10 / D7 | transport switch stop / stepped |
| D3 (GPIO4) | sensor-state LED data via 330 Ω |
| D2 (GPIO3) | transport LED data via 330 Ω |
| 5V | supply for both LEDs through one 1N4148 (each LED with its own 100 nF) |

D6 stays free (it carries the boot messages). D2 is a strapping pin, which is fine for the LED data
output; the optional 10 kΩ pull-down on each LED's data line keeps them dark during a reset. **Only ever connect the sensor to the sensor
socket:** a USB-A charger or port on an A-to-C cable would put 5 V onto the 3V3 line.

## Height (inside 25.4 mm from the rim to the top face)

- XIAO holder: shelf 3.0–4.6, board up to ≈ 9 mm above the rim; switch lugs (14.1 mm + wires) end
  ≈ 9.8 mm above the rim, so keep switches out of the holder's ~25 mm.
- Sensor socket body: cutout centre ≈ 9.5 mm above the rim.

## Development with the XIAO

Upload, log and tests: see the main README, "Development".

## DevKit tray (breadboard board, optional)

[`tray.scad`](tray.scad) holds the 63 × 28 mm DevKit clone in the lid instead, if you ever build the
box with it: plate in the lid recess, located by the corner notches, grooves for clipped pin stubs.
