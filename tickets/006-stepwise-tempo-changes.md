# 006 – Stepped tempo mode and DAW transport

**Status:** implemented – transport and stepped mode still to verify with a DAW (the BOOT button
stands in for the switch until 019)
**Depends on:** 001, 002

Pins are per board in `include/Settings.h`; the values here are the DevKit's (the XIAO's differ).

Hardware checks: see 012 (transport, sync), 013 (stepped mode), 016 (REAPER), 019 (switch wiring).

## Acceptance criteria

### Transport switch (third Velleman 8016, ON-OFF-ON)
- [ ] [hw] Wired on GPIO15 (stop) / GPIO16 (stepped), common to GND. Positions: left = **stop**,
      centre = **run** (tempo glides), right = **run stepped**. Run is in the centre, so flipping between
      the two running modes never passes through stop.
- [ ] [hw] stop → run (or stepped): **Song Position 0** (F2 00 00) then **Start** (FA), sent right after
      a clock tick, so the next tick is beat 1 and the DAW starts from the top in phase.
- [ ] [hw] run ↔ stepped: no transport messages, only the tempo mode changes.
- [ ] [hw] → stop: **Stop** (FC). The MIDI clock keeps running in every mode.
- [ ] [hw] Switch already on at power-on, or USB (re)connects while running: SPP 0 + Start once the host
      listens.
- [x] `TRANSPORT_SWITCH_INSTALLED = false` until wired (an unwired switch reads "run"); the BOOT button
      then cycles stop → run → stepped.
- [x] [log] `transport: stop -> run` lines; plotter `Mode:<-1|0|1>`.

### Stepped mode
- [ ] [hw] The clock tempo is constant for `step_beats` beats (default **16** = 4 bars of 4/4), counted
      from Start, and changes only on the boundary beat.
- [ ] [hw] At each boundary it jumps to the current glide tempo, so a step is what the glide covered in
      that phrase: at most ~1.2 % (slow), ~2.4 % (medium), ~6.4 % (fast) per 16 beats.
- [ ] [log] Plotter `Clock` holds while `Tempo` glides, and catches up on each boundary.
- [ ] [hw] With a DAW synced via clock + SPP, tempo changes land on bar lines only.
- [ ] [hw] REAPER (tempo via CC, `tools/reaper/heart_rate_tempo.lua`) changes tempo only on the
      boundaries.

## Context

The glide is designed to be imperceptible. Stepped mode instead holds the tempo dead steady and changes it
at phrase boundaries, as a band or DJ would. The DAW knows where the phrases are from Song Position +
Start; performers see the count-in on the transport LED (009).

REAPER doesn't follow MIDI clock tempo at all (only SPP position / start / stop), so the device also sends
its tempo as a 14-bit CC pair for the REAPER script. Stepped mode is the clean case for REAPER: it gets one
exact tempo per phrase, so nothing can drift. Glide mode works too, but REAPER's tempo trails the clock by
a few hundred ms and is never re-aligned.

## Notes / Design

- The clock task owns the beat counter (reset when Start is sent) and the held interval; it refreshes the
  held interval from the glide on every `step_beats × 24`-th tick.
- A step is a sudden change; the noticing threshold for steps is ~2–6 % (Drake & Botte 1993), much higher
  than ~0.15–0.27 %/beat for glides. Medium stays around that threshold, fast is deliberately audible.
- Exception to the README invariant "tempo only moves through the glide limit": in stepped mode it jumps
  on boundaries, by at most the glide over one phrase.

## Out of scope / later

- Receiving clock/transport from the DAW (the device stays the clock master).
- A step-length control on the panel: the step length is the runtime setting `step_beats` (010; settings
  page or `set`). Beat flash and count-in are on the transport LED (009).
