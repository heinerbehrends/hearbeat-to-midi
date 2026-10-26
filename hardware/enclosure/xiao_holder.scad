// Holder for a Seeed XIAO ESP32-S3 Plus behind an end wall of a Hammond 1590A,
// so the XIAO's own USB-C port is used through the wall (no extra panel socket).
//
// Glue the holder's back face and tab to the inside of the end wall (epoxy),
// below the USB opening. The board lies on the shelf, USB-C toward the wall, in a
// shallow pocket; tape it down. Its pins along both long sides stay free for
// wires. The tab reaches down into the 1.8 mm gap between end wall and lid lip.
// Print on its side (the back face down), PETG or PLA.
//
// Coordinates: x into the box from the inside face of the end wall, y across
// (centred), z up from the rim (where the lid plate sits).

part = "holder"; // [holder, preview]

/* [XIAO ESP32-S3 Plus: measured] */
xiao_l = 21.0;          // PCB, USB end to the far end
xiao_w = 17.8;
pcb_t = 1.0;            // PCB thickness (measured)
usb_overhang = 2.0;     // how far the USB-C receptacle sticks out past the PCB edge (measured)
usb_above_pcb = 1.6;    // USB-C centre above the PCB's top surface: the receptacle sits
                        // directly on the PCB (measured), half its ~3.2 mm height
fit = 0.2;

/* [Box: Hammond 1590A] */
lip_top = 2.0;          // the lid lip reaches this high into the box...
lip_gap = 1.8;          // ...starting this far from the end wall: (89.1 - 85.5) / 2
clear_w = 22.5;         // free width between the corner bosses at the end walls

/* [Holder] */
shelf_z = 3.0;          // shelf underside above the rim: clears the lid lip
shelf_t = 1.6;
pocket_d = 0.4;         // locating pocket for the PCB
stop_t = 1.6;           // end stop at the far end
tab_t = 1.4;            // glue tab between end wall and lid lip (< lip_gap)
tab_bottom = 0.3;       // tab ends this far above the rim
plug_w = 13.5;          // opening for the USB-C plug's moulding: cut the same into the wall
plug_h = 7.5;

$fn = 32;

// derived
holder_w = xiao_w + 2 * fit + 2 * 1.5;   // 1.5 mm shelf margin each side
pcb_x0 = usb_overhang;                   // receptacle face touches the wall
pcb_z = shelf_z + shelf_t - pocket_d;    // PCB underside
usb_z = pcb_z + pcb_t + usb_above_pcb;   // USB-C centre above the rim
shelf_l = pcb_x0 + xiao_l + fit + stop_t;

assert(holder_w <= clear_w, "holder wider than the space between the corner bosses");
assert(tab_t < lip_gap, "glue tab won't fit between end wall and lid lip");
assert(shelf_z > lip_top, "shelf would sit on the lid lip");
echo(str("USB-C centre is ", usb_z, " mm above the rim: centre the wall opening (",
         plug_w, " x ", plug_h, " mm) there, across the middle of the end wall"));

module holder() {
  difference() {
    union() {
      // shelf
      translate([0, -holder_w / 2, shelf_z]) cube([shelf_l, holder_w, shelf_t]);
      // end stop at the far end, no higher than the PCB's top
      translate([shelf_l - stop_t, -holder_w / 2, shelf_z]) cube([stop_t, holder_w, shelf_t - pocket_d + pcb_t]);
      // glue tab down the wall, into the gap in front of the lid lip
      translate([0, -holder_w / 2, tab_bottom]) cube([tab_t, holder_w, shelf_z - tab_bottom + 0.01]);
    }
    // locating pocket for the PCB
    translate([pcb_x0 - fit, -(xiao_w / 2 + fit), pcb_z]) cube([xiao_l + 2 * fit, xiao_w + 2 * fit, pocket_d + 1]);
  }
}

module preview() {
  color("dimgray") holder();
  color("seagreen") translate([pcb_x0, -xiao_w / 2, pcb_z]) cube([xiao_l, xiao_w, pcb_t]);
  color("silver") translate([0, 0, usb_z]) rotate([0, 90, 0]) linear_extrude(7.5) offset(1.3) square([3.2 - 2.6, 8.9 - 2.6], center = true);
  // end wall with the plug opening
  color("lightgray", 0.4) translate([-1.75, 0, 0]) difference() {
    translate([0, -17.5, 0]) cube([1.75, 35, 25.4]);
    translate([-1, 0, usb_z]) rotate([0, 90, 0]) linear_extrude(4) offset(2) square([plug_h - 4, plug_w - 4], center = true);
  }
}

if (part == "preview") preview();
else holder();
