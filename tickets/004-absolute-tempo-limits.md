# 004 – Absolute clock tempo limits

**Status:** todo
**Depends on:** – (pairs with 001; implement in either order)

## Acceptance criteria

- [ ] Defaults **50–140 BPM** (`clockMinBpm`, `clockMaxBpm`), constants in one place. They sit just
      outside the 55–130 steering domain of 001, so in normal steered use they never engage; they only
      bound follow mode at extreme heart rates and catch bugs.
- [ ] [unit] The glide target is clamped to [50, 140] before the glide, so approaching a limit is smooth:
      steady HR 40 (valid, follow) → tempo settles at 50.0 and never goes below; steady HR 175 → settles at
      140.0. Per-step change still obeys the glide limit.
- [ ] [unit] A pure function (e.g. `float safeClockBpm(float bpm, float lastGood)`) used by `main.cpp`
      right before `intervalFor()` returns a value in [50, 140] for any input, including NaN, ±inf, 0 and
      negatives (NaN/inf → `lastGood`, which itself is clamped; initial `lastGood` = `startBpm`).
- [ ] [unit] `startBpm` (`START_BPM`, 70) lies inside the limits; a `static_assert` or test guards this.
- [ ] [hw] The clock never stops: the interval passed to the clock task is always in
      [~17 857 µs (140 BPM), 50 000 µs (50 BPM)] per tick. Verified by DAW tempo display or by logging the
      min/max interval over a session.
- [ ] [log] If the final safety clamp ever changes a value (i.e. the target clamp failed), print one text
      line `warn: clock tempo clamped <value>` (rate-limited, at most once per 10 s). Plotter line is
      otherwise unchanged.
- [ ] All existing tests still pass (their tempos 70–100 are inside the limits; the 150-bpm test reading
      in `test_tempo_change_stays_below_perception_threshold` stays valid because only the target is
      clamped and the test window ends far below 140).

## Context

The filter validates readings 40–180, and follow mode passes the estimate through, so without a last
clamp the clock could in principle run at 40 or 180 BPM, or at a garbage tempo after a bug (division by
zero in `intervalFor`, NaN from the EMA). This is the safety net that keeps the "clock never stops, never
jumps" promise.

## Notes / Design

- Two layers: (1) clamp the **target** inside `HeartTempo` (smooth, through the glide); (2) a final hard
  clamp in `safeClockBpm()` that should never fire in practice. Layer 2 may jump; that is acceptable for a
  fault path.
- Order with 001: `target = clamp(steer(estimate, fb), clockMin, clockMax)`.
- Keep the 40–180 *validation* range as is – it decides what counts as a reading, not what the clock may
  do.

## Out of scope

- User-adjustable limits.
- Limits on the MIDI clock multiplier (dropped, see README).
