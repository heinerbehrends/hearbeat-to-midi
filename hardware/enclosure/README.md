# Enclosure (Hammond 1590A)

Work in progress. So far: [`tray.scad`](tray.scad), an insulating plate that holds the ESP32 board in
the lid. A drilling template for the top face (three Velleman 8016 switches, LED) and the end walls
(two USB-C panel sockets) follows once those parts are measured.

## Tray

- Lies in the 2 mm recess inside the lid's lip; the lip guides the board's long edges, the corner
  notches around the screw holes locate the plate lengthwise (`notch_r`: measure on the real lid,
  sand the plate ends if it doesn't drop in).
- Board on the plate, taped in the middle. Grooves take the pin stubs: remove the header plastic,
  clip the stubs on the underside to ~0.6 mm, solder wires on the top side.
- End stop at the antenna end, 0.3 mm below the PCB top so the module overhang clears it; open at
  the USB end, so the board's UART port can take a cable with the lid off.
- Print flat, PETG or PLA. Check that the board has no parts on its underside other than the pins.

## Height budget (inside height 25.4 mm from the rim to the top face)

| | above the rim |
| --- | --- |
| tray 1.2 + board 1.6 + tallest parts ~3.2 | ≈ 6.0 mm |
| switch lugs (8016, 14.1 mm, wires soldered from the side) | down to ≈ 9.8 mm |
| USB-C panel sockets in the end walls (body ~5.4 mm tall) | cutout centre ≈ 9.5–11.5 mm, above the board's ends |

Keep the switches ~15 mm away from the end walls (the sockets are 16 mm deep).

## Sockets

- **Laptop (MIDI):** 6-pin USB-C panel socket **with built-in 5.1 kΩ CC resistors**, wired to the
  board's 5V, GND, GPIO19 (D−), GPIO20 (D+); twist D+/D−. Works with C-to-C and A-to-C cables.
- **Sensor:** 4-pin USB-C panel socket (V, D−, D+, G, no CC): V from 3V3, D+ → GPIO17 (SDA),
  D− → GPIO9 (SCL). **Only ever connect the sensor here.** A USB-A charger or port on an A-to-C
  cable would put 5 V onto the 3V3 line.
