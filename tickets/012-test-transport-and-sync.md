# 012 – Manual test: DAW sync, transport and robustness

**Status:** todo
**Depends on:** 006

Manual checks with Ableton Live (or another DAW that follows MIDI clock). Until the transport switch is
wired, `TRANSPORT_SWITCH_INSTALLED = false` and the BOOT button cycles stop → run → stepped.

## Acceptance criteria

### Setup
- [ ] [hw] Native USB port to the laptop (DevKit: also the UART port for the log).
- [ ] [hw] Live: Preferences → Link/Tempo/MIDI → **Sync** on for the device, then **EXT**.
- [ ] [hw] The device shows up as **"Heart Rate To MIDI"** (unplug/replug once if Windows still shows
      "TinyUSB MIDI").

### Transport
- [ ] [hw] → **run**: Live starts at **bar 1**, at ~70 bpm (or the heart rate once locked).
- [ ] [hw] → **stepped**: Live keeps playing, no restart.
- [ ] [hw] → **stop**: Live stops; the clock keeps running (EXT stays active).
- [ ] [hw] Quick stop → run: Live restarts from bar 1.

### Robustness
- [ ] [hw] Unplug and replug USB while running: Live restarts from bar 1 and stays in phase.
- [ ] [hw] Laptop sleep and wake while running: no tempo burst, playback picks up again.
- [ ] [log] `Drops` stays 0 throughout (it may rise only while unplugged).

## Notes

Write down anything odd together with the log lines around it.
