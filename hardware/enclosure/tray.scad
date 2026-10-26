// Insulating tray for the YD-ESP32-S3 (DevKitC-1 clone) in a Hammond 1590A lid.
//
// The 1590A lid is a 2 mm plate with a 2 mm lip that fits into the box; inside
// the lip is a 2 mm deep recess (~82.8 x 28.8 mm). This plate lies in that
// recess and the board lies on the plate, its edges guided by the lip. Grooves
// take the clipped pin stubs, an end stop sits at the antenna end, the USB end
// stays open (plug a cable into the board's UART port with the lid off). Tape the
// board to the plate and the plate into the lid.
// Print flat, PETG or PLA. Coordinates: z = 0 is the inner face of the lid plate.

/* [Board: YD-ESP32-S3, from the vendor drawing] */
board_l = 57.15;         // PCB length (USB end to antenna end)
board_w = 27.94;
pcb_t = 1.6;
row_spacing = 25.40;     // pin rows, centre to centre
antenna_overhang = 6.24; // module sticking out past the PCB at the antenna end

/* [Lid: Hammond 1590A] */
recess_w = 28.8;         // inside the lid lip: 31.5 - 2 x 1.35
lip_h = 2.0;             // the lip, i.e. the recess depth

/* [Plate] */
fit = 0.2;               // clearance per side to the lip
plate_l = 74;            // stays clear of the screw-hole notches in the recess corners
plate_t = 1.2;
groove_w = 3.0;          // grooves for the pin stubs: clip them to ~0.6 mm
groove_d = 0.8;
end_stop_w = 1.6;
end_stop_fit = 0.3;      // play between board end and end stop

$fn = 32;

// derived
plate_w = recess_w - 2 * fit;
cy = plate_w / 2;
// board + antenna centred, so the tall parts on top stay in the middle section
// of the box between its corner bosses
usb_x = (plate_l - (board_l + antenna_overhang)) / 2; // USB edge of the board
stop_x = usb_x + board_l + end_stop_fit;
stop_h = plate_t + pcb_t; // no higher than the PCB top: the module overhangs above it

assert(board_w <= plate_w, "board wider than the lid recess");
assert(usb_x >= 0, "plate too short for the board");
assert(groove_d < plate_t, "grooves would cut through the plate (it insulates the board from the lid)");

difference() {
  union() {
    cube([plate_l, plate_w, plate_t]);
    translate([stop_x, 0, 0]) cube([end_stop_w, plate_w, stop_h]);
  }
  // grooves under both pin rows, running out of the open USB end
  for (s = [-1, 1])
    translate([-1, cy + s * row_spacing / 2 - groove_w / 2, plate_t - groove_d])
      cube([stop_x + 1, groove_w, groove_d + 1]);
}
