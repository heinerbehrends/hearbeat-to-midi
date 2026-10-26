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
| Rate switch | slow / medium / fast (how quickly the tempo moves); dropping it for CC 22 / the settings page is under consideration ([ticket 018](tickets/018-drop-rate-toggle.md)) |
| Transport switch | stop / run / run stepped (until wired: the BOOT button cycles these) |

Two LEDs:

| LED | Shows |
| --- | --- |
| **Sensor** | red blinking = no sensor, red = no finger (tempo held), amber = acquiring, green = following |
| **Transport** | dim white = stopped. Playing: a flash on every beat (brighter on beat 1 of the bar), white = tempo holding, orange = rising, blue = falling. Stepped mode: the 4 beats before a step count in, growing brighter, in the step's colour; the step itself gets a long flash |

Colours, brightness and timing are in `include/Settings.h`.

## Settings at runtime

Most tuning values (rate presets, steering, step length, filter thresholds, LED levels, double time) can
be changed while the device runs, without recompiling: on the **settings page**
(<https://heinerbehrends.github.io/hearbeat-to-midi/>, Chrome or Edge over Web Serial; see
[`docs/README.md`](docs/README.md)), or by typing commands into the serial monitor
(`pio device monitor`; DevKit: UART port, XIAO: its USB port). Each command answers with info lines and
then one `ok …` or `err …` line.

| Command | Does |
| --- | --- |
| `help` | lists the commands |
| `get` | lists every parameter: `param step_beats int 16 min=1 max=64 default=16 "…"` |
| `get <name>` | one parameter |
| `set <name> <value>` | changes it at once (`ok step_beats=32`, or `err step_beats: must be a power of two 1..64`); not saved yet |
| `save` | stores the current values, so they apply after a restart |
| `reset` | all values back to the defaults in `include/Settings.h` (then `save`) |
| `reset <name>` | one value back to its default (then `save`) |

- Changes are logged as `param: step_beats 16 -> 32`. While playing, a new `step_beats` starts on a
  phrase boundary of both lengths and `tempo_multiplier` on a bar line, so the DAW stays in phase.
- Values are checked like the build checks `Settings.h` (ranges, `steer_low_bpm` < `steer_high_bpm`, …).
- Nothing is permanent until `save`: after an accidental `set` or `reset`, a restart brings the saved
  values back. A saved value overrides the one in `Settings.h` until `reset` + `save`; values never changed follow
  `Settings.h`. Pins, the device name and other hardware settings are compile-time only.
- `save` writes flash, which can make one clock tick a few ms late: save while stopped if that matters.
- A few values can also be changed from the DAW by MIDI CC, see [Control from the DAW](#control-from-the-daw).

## DAWs

| DAW | Status |
| --- | --- |
| Ableton Live | works: enable **Sync** for the input, press **EXT** |
| REAPER | doesn't follow MIDI clock tempo; run [`tools/reaper/heart_rate_tempo.lua`](tools/reaper/heart_rate_tempo.lua), which applies the tempo the device also sends as CC. Start/stop via SPP sync. Setup and findings: [`tools/reaper/README.md`](tools/reaper/README.md) |

### Control from the DAW

The DAW can change some settings live by sending control changes on **channel 16** to the device's
MIDI port:

| CC | Changes | Values |
| --- | --- | --- |
| 22 | rate preset | 0–42 slow, 43–85 medium, 86–127 fast; overrides the rate switch until the switch is moved again (the last change wins) |
| 23 | `feedback_offset_bpm` | 0–127 → 0–10 bpm, in 0.1 bpm steps |
| 24 | `tempo_multiplier` | 0–63 → 1, 64–127 → 2 (double time) |

- They work like `set`: the same checks, logged as `param: tempo_multiplier 1 -> 2 (cc24=100)` (the
  rate as `rate: medium -> fast (cc22=127)`), and the same timing (double time on a bar line, while
  stopped at once). The step length isn't a CC (CC 21 was removed): set it on the settings page or with
  `set step_beats`.
- **Not saved:** after a restart the saved values apply again; type `save` to keep the current ones.
- A CC only acts when its value lands in a different zone (or, for CC 23, a different 0.1 bpm), so
  a knob can send as many CCs as it likes. Other channels, the tempo CC pair (20 / 52, which MIDI-thru
  can send back), notes and clock are ignored.
- **Ableton Live:** enable the device's port as a *Track* output (Preferences → Link, Tempo & MIDI),
  set a MIDI track's *MIDI To* to it, channel 16, and draw CC 22–24 in a clip's envelopes (*MIDI Ctrl*);
  or set *MIDI From* to a hardware controller sending these CCs and arm the track.
- **REAPER:** add a track whose output (*Routing → MIDI hardware output*) is the device, and draw CC 22–24
  on channel 16 as CC lanes, or route a controller to it. Leave the device's own input out of that
  track (or keep MIDI-thru off) so the tempo CC doesn't loop back (it is ignored anyway).
- Numbers, channel and zones are in `include/Settings.h` (`CONTROL_CC_*`, `CC_*`).

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
- **XIAO:** its one USB-C port carries MIDI and the serial log (USB CDC).
  `pio run -e xiao-s3-plus -t upload` restarts it into download mode by itself (1200 bps touch; if
  that fails: hold B, tap R, release B). `pio device monitor -e xiao-s3-plus` shows the log;
  `pio test -e xiao-s3-plus -f test_heart_tempo -f test_params` runs the tests on it.
- **Per machine:** copy `platformio_local.ini.example` to `platformio_local.ini` and set the DevKit's
  UART port (`pio device list`, VID:PID 1A86:55D3). The XIAO's port is found automatically.
- `pio run -t upload` flashes the MIDI firmware. `pio run -e diag -t upload` flashes a probe that
  finds the sensor on any pins (DevKit).
- **Tests** run on the board (Unity, `pio test`; `-f <suite>` picks one):

  | Suite | Covers |
  | --- | --- |
  | `test_heart_tempo` | the filter (lock, gate, caution rule, recorded sessions), feedback steering, glide limit, rate changes, switch debounce |
  | `test_params` | settings registry, serial commands, save/load (NVS), step-length and double-time boundaries, DAW CC mapping |
  | `test_smoke` | the board itself: heap and PSRAM size, PSRAM read/write, flash partition size |

- `pio device monitor` shows readable lines (`state: …`, `param: …`, `transport: …`) and, once per
  sensor poll (4 s), a serial-plotter line such as
  `>Raw:72,HR:71.4,Target:66.4,Tempo:68.12,Clock:68.12,State:3,Ok:1,Fb:-1,Rate:0,Mode:0,Drops:0`:

  | Field | Meaning |
  | --- | --- |
  | `Raw` | the sensor's heart rate; left out when it gave none (or its SpO2-valid flag was 0) |
  | `HR` | filtered heart rate estimate |
  | `Target` | where the tempo glides to: `HR` with feedback applied |
  | `Tempo` | glide tempo, in heart rate terms |
  | `Clock` | tempo the clock sends: `Tempo` (held between steps in stepped mode) × `tempo_multiplier`, capped at 240 |
  | `State` | 0 NoSensor, 1 NoFinger, 2 Acquiring, 3 Locked |
  | `Ok` | 1 = the last reading was accepted by the filter |
  | `Fb` | feedback switch: −1 minus, 0 follow, 1 plus |
  | `Rate` | effective rate preset (switch, or CC 22 since the switch last moved): −1 slow, 0 medium, 1 fast |
  | `Mode` | transport: −1 stop, 0 run, 1 stepped |
  | `Drops` | clock ticks the USB stack refused since power-on |
- **All tunable values** (pins, rates, filter, LED, transport) are in
  [`include/Settings.h`](include/Settings.h), with explanations; the ones marked `[runtime]` are defaults
  for the settings above (registry: [`lib/Params`](lib/Params/src/Params.cpp)).

## Troubleshooting (development)

| Problem | Fix |
| --- | --- |
| Upload or test on Windows crashes with an esptool `UnicodeEncodeError` | set `PYTHONIOENCODING=utf-8` before running `pio` (PowerShell: `$env:PYTHONIOENCODING='utf-8'`) |
| Test build fails with Unity "Implicit dependency … unity.h not found" | delete `.pio/build/<env>` and build again (Unity is pinned in `lib_deps` to make this rarer) |
| DevKit held in reset / no log when your own script or tool opens the CH343 port | release DTR and RTS (both false): they drive the auto-reset circuit. `monitor_dtr = 0` / `monitor_rts = 0` in `platformio.ini` do this for `pio device monitor` |
| "Port busy" / "access denied" | only one program can use a serial port at a time: serial monitor, upload, test or the settings page |
| Why `#include "../../../include/Settings.h"` in `lib/Params` | the framework ships its own `settings.h`, and Windows file names are case-insensitive, so `<Settings.h>` can resolve to the wrong file in library builds |

## More

- [Settings page](https://heinerbehrends.github.io/hearbeat-to-midi/) – runtime settings from the
  browser (source and setup: [`docs/`](docs/))
- [`tickets/`](tickets/) – features with acceptance criteria, and what's still open
- [`hardware/sensor_case/`](hardware/sensor_case/) – 3D-printable forehead pod for the sensor
- [`hardware/enclosure/`](hardware/enclosure/) – Hammond 1590A box: XIAO holder, layout, wiring,
  height budget
