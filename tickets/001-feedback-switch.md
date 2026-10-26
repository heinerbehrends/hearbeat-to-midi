# 001 – Feedback switch (follow / − / +)

**Status:** implemented – hardware check pending (switches not wired yet)
**Depends on:** –

Pins are per board in `include/Settings.h`; the values here are the DevKit's (the XIAO's differ).

## Acceptance criteria

- [ ] [hw] Switch on GPIO4 / GPIO5 reads as three positions: both high = **0 (follow)**, GPIO4 low =
      **− (minus)**, GPIO5 low = **+ (plus)**. Both low (wiring fault) is treated as follow.
- [x] [unit] Position decoding + debounce is pure logic (e.g. a `Switch3` helper in `lib/`): a new
      position is reported only after the raw input has been stable for 30 ms; bounces shorter than
      that and the brief centre contact while throwing − → + directly are ignored.
- [x] [unit] Target while Locked: follow → `target = HR` (the smoothed estimate); minus →
      `HR − 5`; plus → `HR + 5`.
- [x] [unit] The steered target is clamped to **55–130 bpm**, without ever being pushed against the
      switch direction: minus → `min(HR, max(HR − 5, 55))`, plus → `max(HR, min(HR + 5, 130))`.
      Examples: HR 58 minus → 55; HR 50 minus → 50; HR 128 plus → 130; HR 135 plus → 135.
- [x] [unit] Flipping the switch never moves the tempo faster than the glide limit: per `update()`
      step, `|Δtempo| / tempo ≤ maxChangePerBeat × beats elapsed` (same check as
      `test_tempo_change_stays_below_perception_threshold`).
- [x] [unit] Steady HR 70, locked, flip to minus: tempo within 0.5 bpm of 65 after 2 min; back to
      follow: within 0.5 of 70 after 2 min. Same for plus → 75.
- [x] [unit] Changing the switch while not Locked does not move the tempo (it still holds); the offset
      takes effect after lock.
- [x] [unit] `heartRate()` is unchanged by the switch (the offset applies to the target only).
- [x] [log] Plotter line gains `Target:<bpm>` and `Fb:<-1|0|1>`; a text line `feedback: follow -> minus`
      is printed on each debounced change and once at boot with the initial position.
- [x] Offset (5), clamp low (55), clamp high (130), debounce (30 ms) and the pins are constants in one
      place.

Hardware checks: see 013 (feedback with the sensor worn) and 020 (wiring in the box).

## Context

Bio-feedback: the clock leads the listener slightly below or above their own heart rate, hoping the
heart rate follows. Today `HeartTempo::update()` glides `tempo_` toward `estimate_`; this adds a steered
target between the two.

## Notes / Design

- Proposed API: `HeartTempo::setFeedback(int dir)` (−1/0/+1) plus `float target() const`; `update()`
  glides toward `target()` instead of `estimate_`. Offset and clamp values as `HeartTempoConfig` fields.
- Keep the switch reading in `loop()` or the sensor task (core 0), never in the clock task. Read the
  switch at least every 10 ms; pass the debounced position to HeartTempo with an atomic or by calling
  the setter from the sensor task.
- Clamp applies to the **steered** positions only. In follow the target is the raw estimate; the absolute
  heart-rate tempo limits from 004 (todo) will apply on top.
- 55–130 is the heart-rate domain where steering makes sense (resting to moderate exertion).

## Hardware

- Velleman 8016, ON-OFF-ON, latching, SPDT. Centre lug → GND. Outer lugs → GPIO4 and GPIO5,
  `INPUT_PULLUP` (internal ~45 kΩ is fine at these lengths). No external parts needed.
- A toggle lever connects the lug on the **opposite** side; if − and + come out swapped, swap the two
  pin constants rather than the wires.
- GPIO4/5 are not strapping pins on the ESP32-S3 (those are 0, 3, 45, 46).

## Out of scope

- A pot for the offset. (The offset is a runtime setting now, `feedback_offset_bpm`, also on CC 23: 010 / 011.)
- Showing the switch position on the LED.
