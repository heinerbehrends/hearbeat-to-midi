# Tickets

Work items for the heartbeat-to-MIDI-clock device (ESP32-S3, PlatformIO, Arduino core 3.x).

| ID  | Title                                                        | Status  | Depends on      |
| --- | ------------------------------------------------------------ | ------- | --------------- |
| 001 | [Feedback switch (follow / − / +)](001-feedback-switch.md)    | todo    | –               |
| 002 | [Rate switch (slow / medium / fast presets)](002-rate-switch.md) | todo | 001             |
| 003 | [External WS2812D status LED on GPIO21](003-external-status-led.md) | todo | –            |
| 004 | [Absolute clock tempo limits](004-absolute-tempo-limits.md)   | todo    | – (pairs with 001) |
| 005 | [Valid-reading rate in the log](005-valid-reading-metric.md)  | backlog | –               |
| 006 | [Stepwise tempo changes on musical boundaries](006-stepwise-tempo-changes.md) | backlog | 001, 002, 003, 004 |
| 007 | [5-pin DIN MIDI out](007-din-midi-out.md)                     | backlog | –               |
| 008 | [BLE heart-rate input (Polar etc.)](008-ble-heart-rate-input.md) | backlog | –            |

## Conventions

- **Status:** `todo` = decided, ready to implement. `backlog` = idea kept for later; scope and numbers
  are a starting point, not a decision. `done` once merged and verified.
- **Acceptance criteria come first** and are checkboxes. Each is tagged with how it is verified:
  - **[unit]** – Unity test in `test/test_heart_tempo` (runs on the device, `pio test`). Anything that
    is pure logic (filter, steering, clamps, debounce decoding) belongs in `lib/` so it can be tested here.
  - **[log]** – visible in the serial-plotter log (`>Name:value,...` lines plus plain-text state lines).
  - **[hw]** – checked on the hardware: LED, switches, a DAW receiving the clock.
- **Tuning values live in one place** (`HeartTempoConfig` / a presets table next to it in
  `lib/HeartTempo/src/HeartTempo.h`, pin numbers at the top of `src/main.cpp`). Tickets name defaults;
  changing a number later should be a one-line edit.
- **Invariants every ticket must keep:**
  - The MIDI clock never stops (clock task on core 1, starts at 90 BPM, holds through gaps).
  - The tempo only ever moves through the glide limit (`maxChangePerBeat`), so no control, preset or
    state change causes an audible jump.
- File names: `NNN-short-name.md`, IDs never reused.

## Won't do / maybe later

**Tempo multiplier (×2 / ×½ clock).** Dropped for now. DAWs already handle double or half time through
note values (play 1/8s instead of 1/4s), so a multiplier adds little musically. If it is ever needed:
dividing a clock is easy downstream (any MIDI utility or DAW can do it), but multiplying is only clean at
the source, since a downstream multiplier has to guess the next tick's timing. So a ×2 option, if it
comes back, belongs in this firmware (scale the clock interval in the clock task, not the HeartTempo
tempo, and apply the limits from 004 to the resulting clock rate).
