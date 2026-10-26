# 002 – Rate switch (slow / medium / fast presets)

**Status:** implemented – hardware check pending (switches not wired yet)
**Depends on:** 001 (reuses the switch decoding/debounce helper)

Pins are per board in `include/Settings.h`; the values here are the DevKit's (the XIAO's differ). Dropping this switch is under
consideration: 018. The rate can also be set by CC 22 (011).

## Acceptance criteria

- [ ] [hw] Second switch on GPIO6 / GPIO7: both high = **medium** (centre), GPIO6 low = **slow**,
      GPIO7 low = **fast**. Both low is treated as medium.
- [x] [unit] Presets come from one table (below). Medium equals today's `HeartTempoConfig` defaults, so
      behaviour with the switch centred is unchanged (all existing tests still pass).
- [ ] [unit] Switching preset never causes a jump: `tempo()` and `heartRate()` immediately after the
      switch equal the values immediately before; from the next `update()` on the per-step change obeys
      the **new** limit (`|Δtempo| / tempo ≤ maxChangePerBeat_new × beats`). (Continuity is covered by
      `test_rate_change_is_continuous_and_applies` / `test_set_config_is_continuous_and_applies`; the
      per-step check with a changed limit is not.)
- [ ] [unit] Mid-glide (HR step 70 → 100, fast preset) switching to slow: tempo keeps rising
      monotonically, never overshoots the target, and the step size drops to the slow limit.
- [ ] [unit] Per preset, with a 70 → 100 HR step: every step obeys that preset's glide limit, and the
      time to get within 0.5 bpm of 100 is ordered fast < medium < slow. (Only fast vs. medium is
      tested so far.)
- [x] [log] Plotter line gains `Rate:<-1|0|1>` (slow/medium/fast); text line `rate: medium -> fast` on each change and once
      at boot (followed by the preset's values, e.g. `(0.400 %/beat, tau 12 s)`).
- [x] Debounce and decoding are identical to 001 (same helper, same 30 ms).

Hardware checks: see 013 / 014 (glide speed, CC 22 override) and 020 (wiring in the box).

| Preset | maxChangePerBeat  | tauSec |
| ------ | ----------------- | ------ |
| slow   | 0.075 % (0.00075) | 40 s   |
| medium | 0.15 % (0.0015)   | 25 s   |
| fast   | 0.4 % (0.004)     | 12 s   |

## Context

Different sessions want different responsiveness: slow = imperceptible drift, fast = clearly audible
following (0.4 %/beat is deliberately above the ~0.17–0.27 %/beat drift-perception thresholds of
Madison & Merker 2004 / Dahl & Granqvist 2003 that the medium value is based on).

## Notes / Design

- Proposed API: `HeartTempo::setRate(float maxChangePerBeat, float tauSec)` or `setPreset(index)` with a
  `kRatePresets[3]` table in `HeartTempo.h`.
- No jump comes for free: `tau` only scales future EMA steps and `maxChangePerBeat` only the next glide
  step; neither touches `estimate_` / `tempo_` directly. Do **not** reset `lastSmoothMs_` or the
  history when the preset changes.
- Interaction with 001: the offset and the rate are independent; the steered target simply approaches
  faster or slower.

## Hardware

- Second Velleman 8016 (ON-OFF-ON SPDT). Centre lug → GND, outer lugs → GPIO6 / GPIO7, `INPUT_PULLUP`.
  Same lever-opposite-lug note as 001.

## Out of scope

- Continuous rate control (pot).
- Separate presets for the outlier gate / consensus (they are runtime settings now, shared by all presets).
