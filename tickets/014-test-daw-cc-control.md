# 014 – Manual test: DAW control over MIDI CC

**Status:** todo
**Depends on:** 011, 012

Manual checks of the incoming control changes (channel 16), with a DAW sending to the device's MIDI
output "Heart Rate To MIDI".

## Acceptance criteria

- [ ] [hw] In the DAW, map a knob or a clip envelope to CC 21–24 on channel 16.
- [ ] [hw] **CC 21** step interval 4 / 8 / 16 / 32: takes effect on a shared phrase boundary.
- [ ] [hw] **CC 22** rate slow / medium / fast: the glide speed audibly changes; moving the rate toggle
      afterwards takes control back.
- [ ] [hw] **CC 23** feedback strength 0–10 bpm: with feedback on − or +, the tempo shifts relative to
      the heart rate.
- [ ] [hw] **CC 24** double time ×1 / ×2: switches on the bar line.
- [ ] [hw] A fast knob sweep: no clock stutters; [log] `Drops` stays 0.
- [ ] [hw] With the device's MIDI routed back to it (MIDI thru), nothing unexpected happens (its own
      tempo CC 20/52 is ignored).
- [ ] [log] Each change is logged (`param: … (ccNN=…)`); none is saved until `save`.
