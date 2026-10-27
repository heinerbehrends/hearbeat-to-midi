# 022 – Manual test: review fixes, settings page redesign, rate preset

**Status:** todo
**Depends on:** 010, 011, 017

Hardware checks for the changes of 2026-10-26/27 on branch `review-fixes` (commits "Review fixes",
"Settings page: main settings up front", "rate_preset"). They build for both boards and the unit tests
compile, but nothing has run on a device yet.

## Acceptance criteria

### Unit tests on the board
- [ ] [unit] `pio test -e xiao-s3-plus -f test_params` passes (new: parse edge cases, load error
      message, save failure, refused `reset <name>`, CC 22 → `rate_preset`, `RateSwitchSource`).
- [ ] [unit] `pio test -f test_params` on the DevKit passes too (same suite, UART port).

### XIAO: settings page over its USB CDC port
- [ ] [hw] Connect the page to the XIAO: the **Set DTR** checkbox (top right) comes up checked and the
      full `get` listing arrives without a timeout (26 parameters, `ok get count=26`).
- [ ] [hw] Uncheck **Set DTR**: nothing arrives any more; after 2 s the "No data from the device yet"
      hint appears; checking it again makes the page load the values.
- [ ] [hw] DevKit on the UART port: the checkbox comes up unchecked and the board is not held in reset.
- [ ] [log] With `pio device monitor -e xiao-s3-plus` a burst of lines (`get`) is complete, no cut
      lines (transmit timeout 20 ms instead of 0).

### Settings page state
- [ ] [hw] Change a value, press **Reload**: the "Unsaved changes" indicator and the field's dot and
      "saved value" note stay.
- [ ] [hw] "Try without a device", Disconnect, then Connect to the real device: the form is empty until
      the device has answered `get`; no demo values show up.
- [ ] [hw] A DAW CC flipping a value there and back (CC 24: 127 then 0, a few seconds apart) leaves
      the header at "All changes saved".
- [ ] [hw] The main group shows tempo multiplier, step length, beats per bar, rate preset, feedback
      offset; everything else is under **More settings**. Change a value inside it, collapse it: the
      summary line says "1 not saved"; an invalid value says "1 with an error".

### Boot with an inconsistent saved set
- [ ] [log] Provoke it: `set step_beats 32`, `set count_in_beats 16`, `save`; then change the
      `STEP_BEATS` default in `Settings.h` to 8 and reflash (step_beats is saved only when it differs
      from its default, so set it back to 16 before saving if you want the rule to trip on the default).
      Boot prints `params: saved values ignored: count_in_beats (16) must be below step_beats (8)`
      before `params: 0 saved value(s) loaded`, and the page shows it as a warning banner. Restore the
      default afterwards, `reset` + `save`.

### Rate preset
- [ ] [hw] Flip the rate switch: the page's **Rate preset** follows, the log shows
      `param: rate_preset 0 -> 1 (switch)` and `rate: medium -> fast (…)`, the header says unsaved.
- [ ] [hw] Pick a preset on the page: the glide speed changes (tempo follows the heart rate faster or
      slower); the Live panel's **Rate** shows it.
- [ ] [hw] Save a preset that differs from the switch position, restart: the saved preset is in use
      (log `rate: fast` at boot, the page shows it) until the switch is moved.
- [ ] [hw] `reset` + `save` (no saved preset), restart with the switch at slow: the boot takes the
      switch position (`param: rate_preset 0 -> -1 (switch)`).
- [ ] [hw] CC 22 from the DAW sets the preset and the page follows; the switch takes over again when
      it is moved; a lane re-sending the same CC value afterwards does nothing (see 014).
- [ ] [hw] `reset rate_preset` on the console answers `ok rate_preset=0`; `reset step_beats` while
      `count_in_beats` is 16 or more is refused with the rule in the error.

## Notes

- Ticket 017 has the general page checks; 014 the CC checks. This ticket only covers what the review
  fixes and the rate preset changed.
- When everything passes, merge `review-fixes` into `main` and mark the `[hw]` items in 010, 011 and
  018 that these cover.
