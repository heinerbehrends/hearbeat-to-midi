# 006 – Stepwise tempo changes on musical boundaries

**Status:** backlog – proposal
**Depends on:** 001, 002, 003, 004
**Estimate:** ~2 days (core stepping ½ d, transport + footswitch ½ d, cue LED ½ d, DAW testing ½ d)

## Acceptance criteria (draft)

### Stepping
- [ ] [unit] In "stepwise" mode the clock tempo is constant for N beats (default N = 32 = 8 bars of 4/4)
      and changes only on the boundary beat (tick count since Start a multiple of N × 24).
- [ ] [unit] The next tempo is decided **4 beats before** the boundary (the current glide target with
      001/004 applied) and does not change after that, so the performer cue (below) is never wrong.
- [ ] [unit] Step size is capped (proposal: ≤ 2 % of tempo or ≤ 3 bpm per boundary, to tune); steps
      smaller than 0.5 bpm are skipped (no change, no cue).
- [ ] [unit] No step while not Locked; the tempo holds, as in glide mode.
- [ ] [log] Text line on every step: `step: 72.0 -> 73.4 at beat 640`.

### DAW sync (transport)
- [ ] [hw] A momentary **footswitch** (to GND, internal pull-up, debounced) toggles transport:
      first press sends **Song Position Pointer 0** (0xF2 0 0) then **MIDI Start** (0xFA) on the next
      clock tick and resets the beat counter there; second press sends **MIDI Stop** (0xFC).
- [ ] [hw] MIDI clock keeps running while stopped (the "clock never stops" rule still holds), so the DAW
      keeps showing the current tempo; only the beat counter and stepping pause.
- [ ] [hw] With a DAW synced to the device, its bar 1 starts on Start and tempo changes land exactly on
      bar lines and nowhere else. Verify with at least the user's DAW.
- [ ] [log] Text lines `transport: start` / `transport: stop`.

### Performer sync (cue LED)
- [ ] [hw] A second WS2812D-F8, **chained** to the status LED's DOUT (no extra GPIO), shows musical
      position while transport runs:
  - short dim white flash on every beat, brighter on each bar's downbeat (every 4 beats);
  - during the **last 4 beats before a step**: flashes in the step's direction colour on each beat as a
    count-in (**blue = slower, orange = faster**);
  - on the boundary beat itself: one long flash in that colour.
- [ ] [hw] While transport is stopped the cue LED is off, so "dark" = not running.
- [ ] Beats per bar (4), N (32), count-in length (4), cap and colours are constants in one place.

## Context

The continuous glide is designed to be imperceptible. This alternative holds the tempo dead steady and
changes it in small steps at phrase boundaries, as a DJ or band would. For that to work musically, the
DAW and the performer must both know where the phrase boundaries are: the DAW through MIDI Start/Song
Position, the performer through the cue LED's count-in.

## Notes / Design

- The clock task already counts ticks (`clockTicks`); stepping needs a counter that resets on Start.
  The clock task should apply both the Start reset and the new tempo on the tick itself, so the step lands
  exactly on the boundary.
- A step is a sudden change: the noticing threshold for steps is ~2–6 % (Drake & Botte 1993), much
  higher than the ~0.15–0.27 %/beat for glides. Capping at ~2 % keeps steps subtle; on a phrase boundary
  a noticeable step may also be wanted.
- Mode selection: proposal is that the rate switch (002) gets a stepwise position, or that a long press on
  the footswitch toggles glide / stepwise. Decide before implementing.
- Footswitch: a standard momentary (non-latching) guitar-style footswitch on a 6.3 mm jack works; check
  how DAWs react to Start while the clock is already running (expected: they start at bar 1).

## Out of scope

- Receiving clock/transport from the DAW (the device stays the clock master).
- Count-in audio (click): the DAW can provide that from the Start position.
