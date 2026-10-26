# 019 – Wire the transport switch

**Status:** todo
**Depends on:** 006

## Acceptance criteria

- [ ] [hw] Third Velleman 8016 (ON-OFF-ON) wired: centre lug to GND, outer lugs to the stop and stepped
      pins of the board (`TRANSPORT_PIN_STOP` / `TRANSPORT_PIN_STEPPED` in `include/Settings.h`):

      | Board | Stop | Stepped |
      | --- | --- | --- |
      | DevKit | GPIO15 | GPIO16 |
      | XIAO | D10 (GPIO9) | D7 (GPIO44) |

- [ ] `TRANSPORT_SWITCH_INSTALLED = true` in `include/Settings.h` (the BOOT button stand-in is then off).
- [ ] [hw] Left = stop, centre = run, right = stepped; if reversed, swap the two pin numbers.
- [ ] [log] Each position logs `transport: … -> …`; `Mode:` follows.
- [ ] Hardware transport checks: see 012 / 013.
