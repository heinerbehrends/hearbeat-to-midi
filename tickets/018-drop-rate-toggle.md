# 018 – Decide whether to drop the rate toggle

**Status:** backlog (decision pending)
**Depends on:** –

## Acceptance criteria

- [ ] Decision recorded here (keep or drop), with the reason.
- [ ] If dropped:
  - [ ] Enclosure layout without the rate switch ([`hardware/enclosure/README.md`](../hardware/enclosure/README.md):
        layout, wiring table; D8 / D9 become free).
  - [x] The rate preset comes from the settings page / `set` (the runtime setting `rate_preset`, saved
        like the others) and CC 22 (done 2026-10-27, with the switch still in: it writes the same setting
        when moved; `RateSwitchSource` in `lib/Params/src/MidiControl.h`). Dropping the switch then only
        removes that source and the pins.
  - [ ] README (controls table, "Control from the DAW"), ticket 002 (marked superseded) and
        `include/Settings.h` (rate pins) updated.
  - [ ] [log] `Rate:` and the `rate: …` line still show the effective preset.

## Context

The rate (slow / medium / fast) is changed rarely and can already be set by CC 22 from the DAW. One
switch fewer frees room on the 1590A's top face and two pins. Against it: no hands-on rate change
without a laptop.
