# 011 – Control from the DAW over MIDI CC

**Status:** implemented – still to check from Ableton / REAPER and while playing
**Depends on:** 010

## Acceptance criteria

### Mappings and filtering
- [x] [unit] Control changes on channel 16 (`CONTROL_CC_CHANNEL`) received on the native USB MIDI port
      change the settings in the table below (numbers, channel and zones in `include/Settings.h`,
      mappings in `lib/Params/src/MidiControl.cpp`).
- [x] [unit] Everything else is ignored: other channels, other CCs, notes, clock, and the tempo CC pair
      the device sends itself (20 / 52, which MIDI-thru in the DAW can send back). A `static_assert`
      keeps the control CCs apart from the tempo CC pair.
- [x] [unit] A CC only acts when its mapped value changes (a knob sends streams of CCs); a value equal to
      the current one changes and logs nothing. A burst is coalesced to the latest value per control.

### Same path as `set`
- [x] [unit] Parameter changes go through `Params::set` (same validation), are not saved (a later `save`
      stores them) and are logged `param: tempo_multiplier 1 -> 2 (cc24=100)`; a refused value logs
      `param: cc24=… refused: <error>`.
- [x] The step length is not a CC (set it on the settings page or with `set step_beats`); CC 21 is ignored.
- [x] The live-apply rules are those of `set`:
      `tempo_multiplier` on the next bar line, while stopped at once; rate and feedback at once
      via `HeartTempo::setConfig` (glide continues).
- [ ] [hw] Double-time changes by CC while playing land on the bar line (as 010): see 014.

### Rate override
- [x] [unit] The effective rate preset is the CC 22 override if one came since the rate switch last
      moved, else the switch position (the last change wins).
- [x] [log] Shown in the `Rate:` plotter field and as `rate: medium -> fast (cc22=127) (…)`.

### Threading and load
- [x] Packets are read in TinyUSB's receive callback (`tud_midi_rx_cb`, USB task), non-blocking, at most
      32 per call (the receive buffer holds 16), and only the latest value per control is handed to the
      sensor task through atomics. The sensor task stays the registry's only writer and applies them every
      100 ms; the clock task is untouched.
- [x] [hw] A flood (2560 CCs from Windows `midiOutShortMsg`) is taken in 0.4 s, gives a handful of log
      lines, and the clock keeps running with `Drops:0`. (Reading from a task instead holds each message
      until the next poll: the host can only send once the 16-packet buffer is drained.)
- [x] [hw] Verified on the DevKit from Windows (winmm): each CC changes its parameter (log and `get`),
      CC 20 / 52, channels 1 and 15 and a note on channel 16 are ignored.

## Controls

The CC table is in the main README, ["Control from the DAW"](../README.md#control-from-the-daw); numbers,
channel and value zones are defined in `include/Settings.h` (`CONTROL_CC_*`, `CC_*`).

## Notes

- Not done: CCs for other parameters, MIDI learn, sending the current values back to the DAW (so
  controller knobs could follow `set`); a knob in the same zone after a serial `set` does nothing until it
  moves to another zone.
- XIAO: builds; untested on hardware (MIDI and serial share its one USB port, the CC path is the same).
