# 016 – Manual test: REAPER

**Status:** todo
**Depends on:** 006, 012

On the Ubuntu machine, with `tools/reaper/heart_rate_tempo.lua` (setup: `tools/reaper/README.md`).

## Acceptance criteria

- [ ] [hw] Running the script: the console says "following the device"; without the device it reports
      that no tempo arrived.
- [ ] [hw] Start/stop via SPP sync starts REAPER at the top and stops it.
- [ ] [hw] Glide mode: REAPER's tempo follows; over a few minutes no audible jumps or re-syncs.
- [ ] [hw] Stepped mode: tempo changes only on the steps.
- [ ] [hw] Changing the tempo by hand in REAPER: within ~2 s the script sets it back.
- [ ] [hw] Running the script again (toolbar toggle) stops it without a prompt.
