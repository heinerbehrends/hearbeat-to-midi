# 013 – Manual test: glide and stepped mode, runtime settings

**Status:** todo
**Depends on:** 006, 010, 012

Manual checks with the sensor worn (finger or earlobe/forehead, locked) and a DAW following the clock.

## Acceptance criteria

### Glide mode
- [ ] [hw] The DAW tempo follows the heart rate slowly and smoothly, no audible jumps.
- [ ] [hw] Breathing more heavily raises the tempo noticeably within about a minute; it settles again
      afterwards.
- [ ] [hw] Removing the sensor: the tempo holds and playback continues.

### Stepped mode
- [ ] [hw] Tempo changes land exactly on the bar line every 16 beats (4 bars), nowhere else.
- [ ] [hw] Note whether the DAW smooths the step (heard as one change, or smeared over a beat or two).
- [ ] [hw] Stepped → run: no jump at the moment of switching; gliding resumes from the next phrase
      boundary.

### Runtime settings (serial console, ticket 010)
- [ ] [hw] `set step_beats 8` while stepped: the next step comes after 2 bars, still on a bar line.
- [ ] [hw] `set tempo_multiplier 2` while playing: the DAW tempo doubles on the next bar line;
      `set tempo_multiplier 1` returns on the next bar line.
- [ ] [hw] Restart without `save`: changes are gone. With `save`: they are kept.
