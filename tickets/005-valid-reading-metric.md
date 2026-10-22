# 005 – Valid-reading rate in the log

**Status:** backlog
**Depends on:** –

## Acceptance criteria

- [ ] [log] Plotter line gains `Valid:<0–100>`: percentage of polls in the last 60 s (15 polls at 4 s)
      that returned a heart rate in the 40–180 range. Polls with the sensor missing count as invalid.
- [ ] [unit] Window logic is pure (ring buffer of the last 15 results): 15 valid → 100; 15 invalid → 0;
      alternating → 53 (8/15) or 47; before 15 polls the percentage is over the polls so far.
- [ ] [log] Optional second field `Acc:<0–100>` = share of polls *accepted* by the filter
      (`lastReadingAccepted`), to separate "sensor gives numbers" from "numbers are believable".
- [ ] [hw] Usable to compare placements: a 3-minute session each on forehead, earlobe and arm gives a
      stable, comparable number in the plotter.

## Context

The SEN0344 returns −1 often; finding the best placement (forehead / earlobe / arm) needs a number, not
eyeballing the raw trace.

## Notes / Design

- Lives next to the existing log in the sensor task; counts per poll, not per `update()`.

## Out of scope

- Storing results, on-device placement wizard.
