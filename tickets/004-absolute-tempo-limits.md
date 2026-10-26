# 004 – Heart-rate tempo limits

**Status:** todo
**Depends on:** – (builds on 001)

## Acceptance criteria

- [ ] Limits on the **heart-rate (glide) tempo**, defaults **50–140 bpm** (`TEMPO_MIN_BPM`,
      `TEMPO_MAX_BPM` in `include/Settings.h`). They sit just outside the 55–130 steering domain of 001,
      so in normal steered use they never engage; they bound follow mode at extreme heart rates.
- [ ] [unit] The glide target is clamped to [50, 140] before the glide, so approaching a limit is smooth:
      steady HR 40 (valid, follow) → tempo settles at 50.0 and never goes below; steady HR 175 → settles at
      140.0. Per-step change still obeys the glide limit.
- [ ] [unit] `START_BPM` (70) lies inside the limits (`static_assert`, and the runtime rule if the
      limits become runtime settings).
- [ ] [unit] Double time is unaffected: the limits apply before `tempo_multiplier`, so 140 × 2 = 280 is
      then capped by the clock (below), not by this ticket.
- [ ] [log] `Target` in the plotter line shows the clamped target.
- [ ] Candidate runtime settings (`tempo_min_bpm`, `tempo_max_bpm`, rule min < max): decide while
      implementing.
- [ ] All existing tests still pass (their tempos 70–100 are inside the limits; the 150-bpm reading in
      `test_tempo_change_stays_below_perception_threshold` stays valid because only the target is
      clamped and the test window ends far below 140).

### Already implemented (the separate last stage, in `src/main.cpp`)
- [x] The clock tempo (after the multiplier) is capped at `MAX_CLOCK_BPM` (240) in `scaleInterval()`.
- [x] `intervalFor()` guards against NaN / inf (→ `START_BPM`) and zero or absurd values (clamped to
      20–300 bpm), so a bug can never stop or flood the clock.

## Context

The filter validates readings 40–180, and follow mode passes the estimate through, so without a limit the
glide could follow a heart rate of 40 or 180. Musically useful tempos are narrower. The hard safety net
(clock cap, NaN guard) already exists and stays separate, so double time can exceed the heart-rate
limits.

## Notes / Design

- Order with 001: `target = clamp(steer(estimate, fb), tempoMin, tempoMax)`, inside `HeartTempo`
  (`HeartTempoConfig` fields), so it goes through the glide.
- Keep the 40–180 *validation* range as is – it decides what counts as a reading, not what the tempo may
  do.

## Out of scope

- Changing the clock cap (`MAX_CLOCK_BPM`) or the `intervalFor()` guard.
