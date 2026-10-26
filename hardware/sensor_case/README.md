# Sensor case (forehead pod)

Holds the DFRobot SEN0344 against the forehead, with black felt on the skin face to keep stray light
out (the most important factor for good readings), and has a panel USB-C socket so the cable can be unplugged (and pops out under a
hard pull, about 8 N, before the headband gets dragged).

Model: [`sensor_case.scad`](sensor_case.scad) (OpenSCAD, free: openscad.org). All dimensions are
parameters at the top of the file; in OpenSCAD open *Window > Customizer* to change them.

## Size

About 27 × 41 × 11.6 mm (plus the strap loops, 6 mm each side). The sensor sits at the bottom end,
the USB-C socket at the top end, so the cable runs up over the head.

The committed `sensor_case.stl` may be from an older version of the model: re-export it after changing
the `.scad` (one STL per part, see below).

## Print

| Part (`part = …`) | Material | Orientation | Notes |
| --- | --- | --- | --- |
| `body` | **opaque black** PETG (or PLA) | **skin face down** (open side up) | no supports needed: the skin plate and strap loops lie on the bed; the only bridge is the top of the USB-C cutout |
| `lid` | same | flat side down | the rib and the four posts point up |
| `felt` | black felt (1–2 mm) | – | 2D template: export as SVG/DXF, print at 100 %, cut the felt to it |

- **Opacity test first:** many black filaments let infrared through, which the sensor uses. Print
  a 2 mm thick test square and hold a phone torch behind it in the dark; if it glows, use another
  filament or increase `wall`.
- 0.2 mm layers, 100 % infill around the window is fine (it's small anyway), 3+ perimeters.

## Parts list

- SEN0344 with its pin headers **removed** (or clipped flush)
- USB-C panel socket, **4P data version (V, D−, D+, G)**, panel cutout 13.6 × 6.3 mm
- 2× M2 × 6 mm self-tapping screws
- 26 mm elastic
- Black felt, 1–2 mm, and glue that stays flexible (contact glue, or double-sided tape)

## Assembly

1. Push the USB-C socket into its cutout from the outside, wires first: the flange stays outside
   and the rubber wedge snaps behind the 2 mm wall.
2. Solder the socket wires to the sensor's I2C pads: V → 3V3, G → GND, D+ → SDA, D− → SCL.
   CC stays unconnected, so USB-C chargers and C-to-C sources won't switch on their 5 V.
   **Only ever connect this socket to the box's sensor socket.** A USB-A charger or port with an
   A-to-C cable always puts 5 V on V, whatever CC says.
3. Lay the sensor glass-down in its pocket, chip in the window. The glass stands `felt_proud`
   (0.5 mm) out of the case face.
4. Glue the felt on the skin face, its hole around the window. With felt much thicker than ~1 mm,
   increase `felt_proud` (about half the felt thickness) so the glass still reaches the skin.
5. Lid on, two screws. Its four posts press the sensor board onto the skin plate (so the glass
   can't lift out of the window), its rib presses the socket onto its cradle. If the posts land on
   parts of the board, move them (`post_inset`) or switch them off (`press_posts`) and use ~8 mm of
   foam instead.
6. Elastic: through the slot next to the body, around the outer bar, back on itself; sew or clip.

## Check before printing

- [ ] Sensor board size, `sensor_total` / `pcb_t` (how far the chip sticks out) measured
- [ ] Felt thickness: `felt_proud` ≈ half of it
- [ ] Window position (`win_dx`, `win_dy`) if the chip isn't centred on the board
- [ ] **Other parts on the sensor side of the board** next to the chip: the board rests on the
      skin-face plate, so anything taller than flat solder needs a recess there
- [ ] Socket dimensions match the one actually bought (`sock_depth` is from the outside of the
      wall to the wire exit)
- [ ] **Back of the sensor board** where the lid posts press (two corners away from the wire pads,
      middle of both long sides): no parts there
