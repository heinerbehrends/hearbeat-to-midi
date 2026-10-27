# 010 – Runtime settings over serial (and double time)

**Status:** implemented – stepped-mode / double-time boundaries still to check while playing into a DAW
**Depends on:** 002, 006, 009

## Acceptance criteria

### Registry and validation
- [x] [unit] A parameter registry (`lib/Params`, pure C++) with name, type (int / float / bool),
      default, min, max and a short description for each runtime setting (table below). Defaults are the
      constants in `include/Settings.h`; the filter configuration built from the defaults equals
      `settings::heartTempoConfig()`.
- [x] [unit] `step_beats` only takes powers of two 1–64, ints only whole numbers, bools
      true/false/on/off/1/0, every value its range.
- [x] [unit] The rules Settings.h checks with `static_assert` are enforced on every change, with a
      message naming the other value: `count_in_beats < step_beats`, `steer_low_bpm < steer_high_bpm`,
      `gate_floor_bpm <= gate_max_bpm`, `invalid_to_no_finger < lock_timeout_polls`. A rejected value
      changes nothing.
- [x] Pins, device name, I2C, sensor polling, tempo CC numbers and the other hardware-tied constants stay
      compile-time only.

### Serial commands
- [x] [unit] `help`, `get`, `get <name>`, `set <name> <value>`, `save`, `reset`, `reset <name>`. Each
      command answers with zero or more info lines (`param …`, `help …`) and exactly one final
      `ok …` / `err …` line. No reply starts with `>` (the plotter log). `\r`, `\n` and `\r\n` all end a
      line; lines over 96 characters are rejected.
- [x] [log] Each change logs `param: <name> <old> -> <new>`; a step length or multiplier change logs
      `clock: step_beats N, tempo_multiplier M in effect` once the clock has applied it.
- [x] [hw] Works on the DevKit's UART port (verified on COM4). XIAO (USB CDC): builds; untested on hardware.

### Persistence
- [x] [unit] `save` stores values in NVS (namespace `hr2midi`) with a schema version; only values that
      differ from their default are stored, so a changed default in Settings.h still applies to values
      never set. Like `set`, `reset` (all) and `reset <name>` apply at once but are only stored by `save`.
- [x] [unit] Loading falls back to the default for missing or invalid values, to all defaults for another
      schema version or a loaded set that breaks a rule.
- [x] [hw] `set`, `save`, reset (RTS pulse), `get` shows the saved values; `reset` + `save` + restart shows the
      defaults again.

### Applying changes live
- [x] [unit] `HeartTempo::setConfig()` keeps tempo, heart rate estimate, state and collected readings;
      the new values apply from the next reading / update (glide limit included). Shrinking `agreeCount`
      is safe.
- [x] The clock task and loop() read live values from atomics (no locks); the sensor task reads the
      serial port (non-blocking, every 100 ms), owns the registry and is its only writer.
- [x] The rate switch still selects slow / medium / fast; the `rate_*` parameters change what each
      position means, applied at once (glide continues). Since 2026-10-27 the preset in use is itself a
      setting, `rate_preset`, changed by the switch (when moved), CC 22 and `set`.
- [x] [unit] A `step_beats` change while playing takes effect on a beat that is a phrase boundary of
      both the old and the new length (every max(old, new) beats from Start), so the phrase grid stays
      in phase with the DAW (`test_step_change_waits_for_common_boundary`). While stopped it applies at
      once.
- [ ] [hw] The same while playing into a DAW: see 013.

### Double time (`tempo_multiplier`)
- [x] [unit] `tempo_multiplier` 1 or 2: clock tempo = heart rate tempo × multiplier. Applied as a scale
      on the clock interval in the clock task; HeartTempo (filter, feedback, glide limit) keeps working in
      heart rate terms.
- [x] [unit] The clock tempo is capped at `MAX_CLOCK_BPM` (240), below the tempo CC's 255.98 bpm limit.
- [x] [log] `Clock:` in the plotter line shows the multiplied tempo (verified: `Tempo:70.00,Clock:140.00`).
- [x] [unit] Switching while playing takes effect on the next bar line (tick count since Start a
      multiple of `beats_per_bar` × 24; `test_multiplier_change_lands_on_a_bar_line`); while stopped at
      once.
- [ ] [hw] While playing into a DAW: lands on the bar line; in stepped mode the held tempo is scaled
      too; the tempo CC (REAPER), beat counting, phrase boundaries and the transport LED's count-in
      follow the clock beats. See 013 / 015 / 016.

## Parameters

As at implementation; for the current list see `get` or `lib/Params/src/Params.cpp`.

| Name | Type | Default | Range |
| --- | --- | --- | --- |
| tempo_multiplier | int | 1 | 1–2 |
| step_beats | int | 16 | power of two 1–64 |
| count_in_beats | int | 4 | 0–63, < step_beats (0 = no count-in) |
| beats_per_bar | int | 4 | 1–16 |
| rate_preset | int | 0 (medium) | −1 slow, 0 medium, 1 fast; the switch position at power-on when not saved |
| rate_slow_max_change / _tau_sec | float | 0.00075 / 40 | 0.0001–0.02 / 1–300 s |
| rate_medium_max_change / _tau_sec | float | 0.0015 / 25 | 0.0001–0.02 / 1–300 s |
| rate_fast_max_change / _tau_sec | float | 0.004 / 12 | 0.0001–0.02 / 1–300 s |
| feedback_offset_bpm | float | 5 | 0–30 |
| steer_low_bpm, steer_high_bpm | float | 55, 130 | 30–220, low < high |
| start_bpm | float | 70 | 40–180 (applies after a restart) |
| caution_above_bpm, caution_step_bpm | float | 100, 8 | 60–180, 1–40 |
| gate_floor_bpm, gate_max_bpm | float | 8, 12 | 1–40, 1–60, floor ≤ max |
| agree_tol_bpm | float | 4 | 0.5–20 |
| lock_timeout_polls | int | 8 | 2–100, > invalid_to_no_finger |
| invalid_to_no_finger | int | 5 | 1–50 |
| led_level | int | 16 | 3–255 |
| led_show_every_miss | bool | true | |
| transport_led_level, stopped_level | int | 16, 3 | 3–255, 1–255 |

## Notes

- Built so the next steps could reuse it, and they do: MIDI CC from the DAW (011) and the Web Serial
  settings page (`docs/`; `get` lists everything with type, range and description, every command ends
  with one `ok` / `err` line).
- `save` writes flash, which pauses flash access on both cores for a moment: a clock tick can come a few
  ms late. Save while stopped if that matters.
- The storage key is the name cut to 15 characters (NVS limit); a unit test keeps them unique.
- Bump `params::SCHEMA_VERSION` when a stored value would mean something different (a unit change);
  adding parameters needs no bump.
