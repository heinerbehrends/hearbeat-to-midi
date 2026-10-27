# Tickets

Work items for the heartbeat-to-MIDI-clock device (ESP32-S3, PlatformIO, Arduino core 3.x).

| ID  | Title                                                        | Status      | Depends on |
| --- | ------------------------------------------------------------ | ----------- | ---------- |
| 001 | [Feedback switch (follow / − / +)](001-feedback-switch.md)    | implemented | –          |
| 002 | [Rate switch (slow / medium / fast presets)](002-rate-switch.md) | implemented | 001     |
| 003 | [External WS2812D status LED on GPIO21](003-external-status-led.md) | implemented | –    |
| 004 | [Heart-rate tempo limits](004-absolute-tempo-limits.md)       | todo        | –          |
| 005 | [Valid-reading rate in the log](005-valid-reading-metric.md)  | backlog     | –          |
| 006 | [Stepped tempo mode and DAW transport](006-stepwise-tempo-changes.md) | implemented | 001, 002 |
| 009 | [Transport LED (beat, tempo direction, count-in)](009-transport-led.md) | implemented | 003, 006 |
| 010 | [Runtime settings over serial (and double time)](010-runtime-settings.md) | implemented | 002, 006, 009 |
| 011 | [Control from the DAW over MIDI CC](011-daw-control.md)        | implemented | 010        |
| 012 | [Manual test: DAW sync, transport, robustness](012-test-transport-and-sync.md) | todo | 006 |
| 013 | [Manual test: glide, stepped, runtime settings](013-test-glide-and-stepped.md) | todo | 006, 010, 012 |
| 014 | [Manual test: DAW control over CC](014-test-daw-cc-control.md) | todo | 011, 012 |
| 015 | [Manual test: both LEDs](015-test-leds.md) | todo | 003, 009, 012 |
| 016 | [Manual test: REAPER](016-test-reaper.md) | todo | 006, 012 |
| 017 | [Manual test: web settings page](017-test-web-settings-page.md) | todo | 010, 011 |
| 018 | [Decide whether to drop the rate toggle](018-drop-rate-toggle.md) | backlog | – |
| 019 | [Wire the transport switch](019-wire-transport-switch.md) | todo | 006 |
| 020 | [Build the 1590A enclosure, verify the XIAO](020-build-enclosure-xiao.md) | todo | 018 |
| 021 | [Print the sensor case](021-print-sensor-case.md) | todo | – |
| 022 | [Manual test: review fixes, page redesign, rate preset](022-test-review-fixes-and-rate-preset.md) | todo | 010, 011, 017 |

## Conventions

- **Status:**
  - `backlog` = idea kept for later; scope and numbers are a starting point, not a decision.
  - `todo` = decided, ready to implement.
  - `implemented` = in the firmware and unit-tested; open `[hw]` criteria are checked on the hardware
    in the manual test tickets 012–017.
  - `done` = implemented and every criterion verified.
- **Acceptance criteria come first** and are checkboxes. Each is tagged with how it is verified:
  - **[unit]** – Unity test in `test/test_heart_tempo` or `test/test_params` (runs on the device, `pio test`). Anything that
    is pure logic (filter, steering, clamps, debounce decoding) belongs in `lib/` so it can be tested here.
  - **[log]** – visible in the serial-plotter log (`>Name:value,...` lines plus plain-text state lines).
  - **[hw]** – checked on the hardware: LED, switches, a DAW receiving the clock.
- **Tuning values live in one place:** `include/Settings.h` (pins, presets, filter, LED). Tickets name
  defaults; changing a number later should be a one-line edit. Values marked `[runtime]` there are
  defaults that can also be changed at runtime (serial or settings page, 010); a new tunable should join `lib/Params` when it is
  safe to change while running.
- **Invariants every ticket must keep:**
  - The MIDI clock never stops (clock task on core 1, starts at `START_BPM` = 70, holds through gaps).
  - The tempo only ever moves through the glide limit (`maxChangePerBeat`), so no control, preset or
    state change causes an audible jump. Exceptions: stepped mode (006) jumps on phrase boundaries,
    by at most what the glide covered in that phrase; a `tempo_multiplier` change (010) doubles or
    halves the clock on a bar line.
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

**Half time (×½ clock).** Not planned: dividing a clock is easy downstream (any MIDI utility or DAW can
do it). Double time is implemented as the runtime setting `tempo_multiplier` (010): multiplying is only
clean at the source, since a downstream multiplier has to guess the next tick's timing, so it scales the
clock interval in the clock task (not the HeartTempo tempo), capped at `MAX_CLOCK_BPM` (240). 004 limits
the heart-rate (glide) tempo before the multiplier; the clock cap and the guard in `intervalFor()` stay
the separate last stage, so double time keeps working.
