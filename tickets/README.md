# Tickets

Work items for the heartbeat-to-MIDI-clock device (ESP32-S3, PlatformIO, Arduino core 3.x).

| ID  | Title                                                        | Status      | Depends on |
| --- | ------------------------------------------------------------ | ----------- | ---------- |
| 001 | [Feedback switch (follow / − / +)](001-feedback-switch.md)    | implemented | –          |
| 002 | [Rate switch (slow / medium / fast presets)](002-rate-switch.md) | implemented | 001     |
| 003 | [External WS2812D status LED on GPIO21](003-external-status-led.md) | implemented | –    |
| 004 | [Absolute clock tempo limits](004-absolute-tempo-limits.md)   | todo        | –          |
| 005 | [Valid-reading rate in the log](005-valid-reading-metric.md)  | backlog     | –          |
| 006 | [Stepped tempo mode and DAW transport](006-stepwise-tempo-changes.md) | implemented | 001, 002 |
| 009 | [Transport LED (beat, tempo direction, count-in)](009-transport-led.md) | implemented | 003, 006 |

## Conventions

- **Status:**
  - `backlog` = idea kept for later; scope and numbers are a starting point, not a decision.
  - `todo` = decided, ready to implement.
  - `implemented` = in the firmware and unit-tested; the `[hw]` criteria still need checking on the
    hardware (switches and LED aren't wired yet).
  - `done` = implemented and every criterion verified.
- **Acceptance criteria come first** and are checkboxes. Each is tagged with how it is verified:
  - **[unit]** – Unity test in `test/test_heart_tempo` (runs on the device, `pio test`). Anything that
    is pure logic (filter, steering, clamps, debounce decoding) belongs in `lib/` so it can be tested here.
  - **[log]** – visible in the serial-plotter log (`>Name:value,...` lines plus plain-text state lines).
  - **[hw]** – checked on the hardware: LED, switches, a DAW receiving the clock.
- **Tuning values live in one place:** `include/Settings.h` (pins, presets, filter, LED). Tickets name
  defaults; changing a number later should be a one-line edit.
- **Invariants every ticket must keep:**
  - The MIDI clock never stops (clock task on core 1, starts at `START_BPM` = 70, holds through gaps).
  - The tempo only ever moves through the glide limit (`maxChangePerBeat`), so no control, preset or
    state change causes an audible jump. Exception: stepped mode (006) jumps on phrase boundaries,
    by at most what the glide covered in that phrase.
- File names: `NNN-short-name.md`, IDs never reused (007 and 008 were used for backlog ideas that
  were dropped: 5-pin DIN MIDI out, BLE heart-rate input).

## Implemented without a ticket

Found while testing with the real sensor, or in code reviews:

- **Sensor junk rejection:** the heart rate counts as missing while the sensor's SpO2-valid flag is 0
  (`REQUIRE_SPO2_VALID`); readings above 100 bpm only pass as a smooth series or when they agree
  (`CAUTION_*`); the outlier gate is capped (`GATE_MAX_BPM`); a lock that accepts nothing for
  `LOCK_TIMEOUT_POLLS` drops back to acquiring. Recorded sessions are regression tests.
- **Start tempo 70 bpm** (`START_BPM`).
- **Tempo as MIDI CC for REAPER**, which doesn't follow clock tempo:
  `tools/reaper/heart_rate_tempo.lua`.
- **Robust MIDI output:** retries when the USB buffer is full, the phrase count follows only ticks
  that went out, resync after lost beats, pending Start/Stop.
- **Per-machine serial ports** in `platformio_local.ini` (see `platformio_local.ini.example`).

## Won't do / maybe later

**Tempo multiplier (×2 / ×½ clock).** Dropped for now. DAWs already handle double or half time through
note values (play 1/8s instead of 1/4s), so a multiplier adds little musically. If it is ever needed:
dividing a clock is easy downstream (any MIDI utility or DAW can do it), but multiplying is only clean at
the source, since a downstream multiplier has to guess the next tick's timing. So a ×2 option, if it
comes back, belongs in this firmware (scale the clock interval in the clock task, not the HeartTempo
tempo, and apply the limits from 004 to the resulting clock rate).
