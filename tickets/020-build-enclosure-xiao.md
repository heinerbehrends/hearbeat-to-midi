# 020 – Build the 1590A enclosure and verify the XIAO on hardware

**Status:** todo
**Depends on:** [`hardware/enclosure/README.md`](../hardware/enclosure/README.md), 018 (switch layout)

## Acceptance criteria

- [ ] [hw] XIAO holder printed and glued in end wall A; a USB-C cable seats through the wall opening.
- [ ] [hw] MIDI and the serial log (USB CDC) work at the same time over the XIAO's one port: the DAW
      sees "Heart Rate To MIDI" while `pio device monitor -e xiao-s3-plus` shows the log.
- [ ] [hw] `pio run -e xiao-s3-plus -t upload` works without buttons (1200 bps touch).
- [ ] [hw] Pins as in the wiring table: each switch position shows in the log (`Fb`, `Rate`, `Mode`).
- [ ] [hw] Both LEDs work (sensor LED on D3, transport LED on D2), stay dark during a reset.
- [ ] [hw] Sensor socket wired V / G / D+ (SDA) / D− (SCL) with the 4.7 kΩ pull-ups; the sensor locks.
- [ ] [hw] Tests pass on the XIAO: `pio test -e xiao-s3-plus`.
- [ ] Update the "XIAO untested" notes in tickets 010 and 011 once verified.

Further hardware checks: 012–017, run once on the XIAO build.
