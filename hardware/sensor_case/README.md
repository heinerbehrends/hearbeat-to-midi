# Sensor case (forehead pod)

Holds the DFRobot SEN0344 against the forehead, with black felt on the skin face to keep stray light
out (the most important factor for good readings), and has a panel USB-C socket so the cable can be unplugged (and pops out under a
hard pull, about 8 N, before the headband gets dragged).

Model: [`sensor_case.scad`](sensor_case.scad) (OpenSCAD, free: openscad.org). All dimensions are
parameters at the top of the file; in OpenSCAD open *Window > Customizer* to change them.

## Size

About 27 × 43 × 11.6 mm (plus the strap loops, 6 mm each side). The sensor sits at the bottom end,
the USB-C socket at the top end, so the cable runs up over the head.

## Print

| Part (`part = …`) | Material | Orientation | Notes |
| --- | --- | --- | --- |
| `body` | **opaque black** PETG (or PLA) | open side down (skin face up) | no supports needed; the strap slots are vertical |
| `lid` | same | flat side down | |
| `felt` | black felt (1–2 mm) | – | 2D template: export as SVG/DXF, print at 100 %, cut the felt to it |

- **Opacity test first:** many black filaments let infrared through, which the sensor uses. Print
  a 2 mm thick test square and hold a phone torch behind it in the dark; if it glows, use another
  filament or increase `wall`.
- 0.2 mm layers, 100 % infill around the window is fine (it's small anyway), 3+ perimeters.

## Parts list

- SEN0344 with its pin headers **removed** (or clipped flush)
- USB-C panel socket, **4P data version (V, D−, D+, G)**, panel cutout 13.6 × 6.3 mm
- 2× M2 × 6 mm self-tapping screws
- A piece of soft foam (~3 mm) between the back of the sensor board and the lid
- 26 mm elastic
- Black felt, 1–2 mm, and glue that stays flexible (contact glue, or double-sided tape)

## Assembly

1. Push the USB-C socket into its cutout from the outside, wires first: the flange stays outside
   and the rubber wedge snaps behind the 2 mm wall.
2. Solder the socket wires to the sensor's I2C pads: V → 3V3, G → GND, D+ → SDA, D− → SCL.
   Leave CC unconnected (charge-only cables and chargers then can't do anything).
3. Lay the sensor glass-down in its pocket, chip in the window. The glass stands `felt_proud`
   (0.5 mm) out of the case face.
4. Glue the felt on the skin face, its hole around the window. With felt much thicker than ~1 mm,
   increase `felt_proud` (about half the felt thickness) so the glass still reaches the skin.
5. Foam on the back of the sensor board, lid on, two screws. The rib under the lid presses the
   socket onto its cradle.
6. Elastic: through the slot next to the body, around the outer bar, back on itself; sew or clip.

## Check before printing

- [ ] Sensor board size, `sensor_total` / `pcb_t` (how far the chip sticks out) measured
- [ ] Felt thickness: `felt_proud` ≈ half of it
- [ ] Window position (`win_dx`, `win_dy`) if the chip isn't centred on the board
- [ ] **Other parts on the sensor side of the board** next to the chip: the board rests on the
      skin-face plate, so anything taller than flat solder needs a recess there
- [ ] Socket dimensions match the one actually bought
