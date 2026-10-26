# 021 – Print the sensor case

**Status:** todo
**Depends on:** [`hardware/sensor_case/README.md`](../hardware/sensor_case/README.md)

## Acceptance criteria

- [ ] STL re-exported from `sensor_case.scad`: body and lid as separate files, plus the felt template
      (SVG/DXF). The committed `sensor_case.stl` may predate the current model.
- [ ] Opacity test: a 2 mm test square of the chosen filament doesn't glow with a phone torch behind it.
- [ ] "Check before printing" list in the sensor case README done (board size, felt, window, parts).
- [ ] Fit check: sensor board, USB-C socket and lid fit; the glass stands proud of the felt.
- [ ] The lid posts land on bare board, not on parts (else move them with `post_inset` or use foam).
- [ ] [hw] Worn on the forehead with the elastic: the sensor locks (`State:3`) within a minute.
