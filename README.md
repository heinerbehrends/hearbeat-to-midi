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

## Controls and LED

| Control | Positions |
| --- | --- |
| Feedback switch | − / follow / + |
| Rate switch | slow / medium / fast (how quickly the tempo moves) |
| Transport switch | stop / run / run stepped (until wired: the BOOT button cycles these) |

LED: red blinking = no sensor, red = no finger (tempo held), amber = acquiring, green = following.

## DAWs

| DAW | Status |
| --- | --- |
| Ableton Live | works: enable **Sync** for the input, press **EXT** |
| REAPER | doesn't follow MIDI clock tempo; run [`tools/reaper/heart_rate_tempo.lua`](tools/reaper/heart_rate_tempo.lua), which applies the tempo the device also sends as CC (setup in the script header). Start/stop via SPP sync. |

## Development

- PlatformIO (VS Code extension or CLI). The board's two USB-C ports:
  - **native port:** USB MIDI only (to the DAW)
  - **UART port (CH343):** upload, serial log, tests; no BOOT/RST buttons needed
- **Per machine:** copy `platformio_local.ini.example` to `platformio_local.ini` and set the UART
  port (`pio device list`, VID:PID 1A86:55D3).
- `pio run -t upload` flashes the MIDI firmware. `pio test -f test_heart_tempo` runs the filter tests
  on the board. `pio run -e diag -t upload` flashes a probe that finds the sensor on any pins.
- `pio device monitor` shows a serial-plotter log (`>HR:…,Tempo:…`) plus readable state lines.
- **All tunable values** (pins, rates, filter, LED, transport) are in
  [`include/Settings.h`](include/Settings.h), with explanations.

## More

- [`tickets/`](tickets/) – features with acceptance criteria, and what's still open
- [`hardware/sensor_case/`](hardware/sensor_case/) – 3D-printable forehead pod for the sensor
- [`hardware/enclosure/`](hardware/enclosure/) – Hammond 1590A box: board tray, height budget,
  socket wiring
