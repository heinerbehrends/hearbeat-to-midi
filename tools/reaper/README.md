# REAPER tempo script

[`heart_rate_tempo.lua`](heart_rate_tempo.lua) makes REAPER follow the device's tempo.

## Why

REAPER follows only the **Song Position, Start and Stop** of an external MIDI source, not the tempo of
its MIDI clock. So the device also sends its tempo (the tempo the clock actually sends, multiplier
included) as a 14-bit CC pair, and this script applies it:

| | |
| --- | --- |
| Channel | 16 (`TEMPO_CC_CHANNEL`) |
| CC 20 | MSB (`TEMPO_CC`) |
| CC 52 | LSB (`TEMPO_CC` + 32); the value is applied on the LSB |
| Value | bpm × 64 (1/64 bpm resolution, up to 255.98 bpm) |
| Sent | on every change, every 2 s unchanged (`TEMPO_CC_RESEND_MS`), and when the host connects |

The script sets the tempo of the project's **first tempo marker** (creating one at the project start if
there is none). Numbers in `include/Settings.h` and at the top of the script must match.

## Setup (once)

- Preferences → MIDI Devices: enable **"Heart Rate To MIDI"** as an input ("control only" is enough).
- Disable audio time stretching for items that should not follow the tempo.
- Keep a single tempo marker: later markers override the one the script sets.
- For start/stop: right-click Play → external timecode synchronization, SPP from that input. The
  device's transport switch (or its BOOT button as a stand-in) sends SPP + Start/Stop.

## Run and stop

Actions → Load ReaScript, then run it; it works best as a **toolbar toggle**. It keeps running in the
background; running it again stops it without a prompt. Needs REAPER 6.41 or newer
(`MIDI_GetRecentInputEvent`).

| Console message | Means |
| --- | --- |
| `following the device, 72.00 bpm` | the first tempo arrived |
| `no tempo received yet. Is "Heart Rate To MIDI" enabled as a MIDI input …` | nothing after 5 s: check the input, the cable and the device name (`DEVICE_NAME = nil` accepts any input) |
| `the project has N tempo markers; only the first follows the heart rate` | remove the extra markers |

## Findings

- `SetTempoTimeSigMarker` on the first marker works during playback with audio time stretching
  disabled; `SetCurrentBPM` worked worse.
- **Stepped mode is the clean case:** one exact tempo per phrase, applied on the phrase boundary.
- In glide mode REAPER's tempo trails the clock slightly (a few hundred ms) and is never re-aligned.

## Limitations

- Only the first tempo marker follows; tempo maps with several markers aren't supported.
- A manual tempo change in REAPER is overwritten by the next resend (within ~2 s).
- Differences under 0.05 bpm are ignored (`MIN_CHANGE_BPM`); tempos under 20 bpm are ignored.
- Without the device's input enabled (or with a different device name) nothing happens apart from the
  warning.
