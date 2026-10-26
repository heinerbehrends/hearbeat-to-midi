# 009 – Transport LED (stopped / beat / tempo direction / count-in)

**Status:** implemented – hardware check pending (LED not wired yet)
**Depends on:** 003, 006

## Acceptance criteria

- [ ] [hw] A second WS2812D-F8 on its own data pin (XIAO D2 / DevKit GPIO10, via 330 Ω), sharing the
      5V-through-diode supply with the sensor LED. The sensor LED keeps showing only the sensor state.
- [ ] [hw] Stopped: dim white, steady.
- [ ] [hw] Playing: a short flash on every beat, in phase with the DAW (counted from Start: the first
      clock tick after Start is beat 1), brighter on beat 1 of each bar (`BEATS_PER_BAR`).
- [ ] [hw] Glide mode: white while the tempo holds, orange while it rises, blue while it falls (only
      once the target is ≥ `DIRECTION_MIN_BPM` away, so it doesn't flicker).
- [ ] [hw] Stepped mode: during the last `COUNT_IN_BEATS` (4) beats before a phrase boundary the
      flashes take the colour of the coming step and grow brighter; no count-in for steps smaller than
      `STEP_CUE_MIN_BPM`. On the boundary itself one long flash (`STEP_FLASH_MS`) in the colour of the
      step just made.
- [ ] Colours, levels, timing and thresholds are constants in `include/Settings.h`.

## Notes

- The clock task publishes ticks since Start and the direction of the coming / last step; the sensor
  task publishes the glide direction (target vs. tempo while locked). `updateTransportLed()` in
  `src/main.cpp` turns that into colours.
- Possible later: a different count-in length, or showing the step size by brightness.
