# Heartbeat to midi
Trying to build a device that reads heartbeat rate and outputs midi clock, with the possibility of bio-feedback loops

A heart rate sensor (DFRobot SEN0344, worn on the forehead or earlobe) feeds an ESP32-S3, which sends
USB MIDI clock whose tempo slowly follows the heart rate. It shows up as **"Heart Rate To MIDI"**.

- **Glide mode:** the tempo follows at most 0.15 % per beat, just below what listeners notice in a
  continuous tempo drift.
- **Stepped mode:** the tempo holds for 16 beats and changes only on the phrase boundary.
- **Feedback:** − / 0 / + steers the tempo 5 bpm below or above the heart rate (55–130 bpm).
- **Transport:** Song Position + Start / Stop, so the DAW starts at bar 1 in phase. The clock itself
  never stops.
- **Double time:** the setting `tempo_multiplier 2` doubles the clock tempo (switches on a bar line).

## Controls and LED

| Control | Positions |
| --- | --- |
| Feedback switch | − / follow / + |
| Rate switch | slow / medium / fast (how quickly the tempo moves) |
| Transport switch | stop / run / run stepped (until wired: the BOOT button cycles these) |

Two LEDs:

| LED | Shows |
| --- | --- |
| **Sensor** | red blinking = no sensor, red = no finger (tempo held), amber = acquiring, green = following |
| **Transport** | dim white = stopped. Playing: a flash on every beat (brighter on beat 1 of the bar), white = tempo holding, orange = rising, blue = falling. Stepped mode: the 4 beats before a step count in, growing brighter, in the step's colour; the step itself gets a long flash |

Colours, brightness and timing are in `include/Settings.h`.

## Settings at runtime

Most tuning values (rate presets, steering, step length, filter thresholds, LED levels, double time) can
be changed while the device runs, without recompiling: type commands into the serial monitor
(`pio device monitor`; DevKit: UART port, XIAO: its USB port). Each command answers with info lines and
then one `ok …` or `err …` line.

| Command | Does |
| --- | --- |
| `help` | lists the commands |
| `get` | lists every parameter: `param step_beats int 16 min=1 max=64 default=16 "…"` |
| `get <name>` | one parameter |
| `set <name> <value>` | changes it at once (`ok step_beats=32`, or `err step_beats: must be a power of two 1..64`); not saved yet |
| `save` | stores the current values, so they apply after a restart |
| `reset` | all values back to the defaults in `include/Settings.h`, saved |
| `reset <name>` | one value back to its default (then `save`) |

- Changes are logged as `param: step_beats 16 -> 32`. While playing, a new `step_beats` starts on a
  phrase boundary of both lengths and `tempo_multiplier` on a bar line, so the DAW stays in phase.
- Values are checked like the build checks `Settings.h` (ranges, `steer_low_bpm` < `steer_high_bpm`, …).
- A saved value overrides the one in `Settings.h` until `reset`; values never changed follow
  `Settings.h`. Pins, the device name and other hardware settings are compile-time only.
- `save` writes flash, which can make one clock tick a few ms late: save while stopped if that matters.

## DAWs

| DAW | Status |
| --- | --- |
| Ableton Live | works: enable **Sync** for the input, press **EXT** |
| REAPER | doesn't follow MIDI clock tempo; run [`tools/reaper/heart_rate_tempo.lua`](tools/reaper/heart_rate_tempo.lua), which applies the tempo the device also sends as CC (setup in the script header). Start/stop via SPP sync. |

## Boards

The same code builds for two boards; the PlatformIO **environment** picks the board and its pins.

| Environment | Board | Used for |
| --- | --- | --- |
| `esp32-s3-n16r8` (**default**) | ESP32-S3 DevKitC-1 clone (YD-ESP32-S3, two USB-C ports) | breadboard development |
| `xiao-s3-plus` | Seeed XIAO ESP32-S3 Plus (one USB-C port) | the build in the Hammond 1590A |

- Plain `pio run -t upload` (or the VS Code Upload button) builds the default, the DevKit. For the
  XIAO: `pio run -e xiao-s3-plus -t upload`, or pick `env:xiao-s3-plus` in the PlatformIO status bar
  first. To make the XIAO the default, set `default_envs = xiao-s3-plus` in `platformio.ini`.
- **Pins** are set per board in one block at the top of [`include/Settings.h`](include/Settings.h)
  (`#if defined(BOARD_XIAO)` … `#else` …). **Everything else in Settings.h is shared**, so a tweak
  applies to both boards.
- XIAO wiring and the box layout: [`hardware/enclosure/README.md`](hardware/enclosure/README.md).

## Development

- PlatformIO (VS Code extension or CLI).
- **DevKit:** two USB-C ports.
  - **native port:** USB MIDI only (to the DAW)
  - **UART port (CH343):** upload, serial log, tests; no BOOT/RST buttons needed
- **XIAO:** its one USB-C port carries MIDI and the serial log (USB CDC); uploads restart it into
  download mode by themselves (if that fails: hold B, tap R, release B).
- **Per machine:** copy `platformio_local.ini.example` to `platformio_local.ini` and set the DevKit's
  UART port (`pio device list`, VID:PID 1A86:55D3). The XIAO's port is found automatically.
- `pio run -t upload` flashes the MIDI firmware. `pio test -f test_heart_tempo -f test_params` runs the
  filter and settings tests on the board (add `-e xiao-s3-plus` for the XIAO). `pio run -e diag -t upload` flashes a probe that
  finds the sensor on any pins (DevKit).
- `pio device monitor` shows a serial-plotter log (`>HR:…,Tempo:…`) plus readable state lines.
- **All tunable values** (pins, rates, filter, LED, transport) are in
  [`include/Settings.h`](include/Settings.h), with explanations; the ones marked `[runtime]` are defaults
  for the settings above (registry: [`lib/Params`](lib/Params/src/Params.cpp)).

## More

- [`tickets/`](tickets/) – features with acceptance criteria, and what's still open
- [`hardware/sensor_case/`](hardware/sensor_case/) – 3D-printable forehead pod for the sensor
- [`hardware/enclosure/`](hardware/enclosure/) – Hammond 1590A box: XIAO holder, layout, wiring,
  height budget
