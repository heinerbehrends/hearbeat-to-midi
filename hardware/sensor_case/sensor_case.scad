// Forehead pod for the DFRobot SEN0344 heart rate sensor with a panel USB-C socket.
//
// Parts (choose with `part` below):
//   body  - STL, opaque black PETG/PLA; black felt is glued on the skin face
//   lid   - STL, same material, screwed on with 2x M2 self-tapping screws
//   felt  - 2D template (export as SVG/DXF, print 1:1) for cutting the felt
//   all   - exploded preview of everything
//
// Coordinates: x across the forehead, y up toward the hairline (USB-C socket on
// top), z away from the skin (z = 0 is the skin face). All sizes in mm.
// See README.md next to this file for printing and assembly.

part = "all"; // [all, body, lid, felt]

/* [Sensor module (DFRobot SEN0344): measure and adjust] */
sensor_w = 22;      // board side along x
sensor_h = 18;      // board side along y; the edge with the wire pads faces the socket (+y)
sensor_total = 3.0; // back of the board to the sensor glass
pcb_t = 1.5;        // board thickness (the glass sticks out sensor_total - pcb_t)
win_w = 6;          // optical window (the sensor chip) along x
win_h = 4;          // ... along y
win_dx = 0;         // window centre offset from the board centre along x
win_dy = 0;         // ... along y (+ = toward the socket)
fit = 0.3;          // clearance per side for pockets and openings

/* [USB-C panel socket (4P data, V / D- / D+ / G)] */
cut_w = 13.6;       // panel cutout from the datasheet
cut_h = 6.3;
cut_r = 1.3;
sock_w = 12.0;      // socket body behind the panel
sock_h = 5.3;
sock_depth = 14;    // flange to wire exit
flange_w = 16.7;    // only used for the preview
flange_h = 10.3;
socket_wall = 2.0;  // wall the socket snaps into (its rubber wedge expects ~2 mm)

/* [Case] */
wall = 2.2;         // >= 2 mm keeps infrared light out with most black filaments
body_h = 10;        // body height without the lid (>= flange_h - lid_t)
lid_t = 1.6;
wire_gap = 6;       // room between sensor pads and socket wires
// Curvature of the skin face across the forehead (0 = flat). The felt follows the
// forehead well enough on a flat face; a curve needs a thicker skin-face plate
// (curvature eats ~w^2/(8r) at the board edges).
forehead_r = 0;
boss_d = 5;         // screw bosses for the lid
screw_pilot = 1.7;  // pilot hole for M2 self-tapping screws
screw_clear = 2.4;  // clearance hole in the lid

/* [Strap loops] */
strap_w = 26;       // elastic width
strap_slot = 2.6;   // slot width for the elastic thickness
loop_bar = 3.2;     // width of the bar the elastic wraps around
loop_h = 5;         // height of the loop bar (z)

/* [Felt on the skin face] */
// The felt covers the skin face and blocks stray light. The sensor glass stands
// this far out of the case face, into the felt hole, so it touches the skin once
// the felt is lightly compressed. Roughly half the felt thickness.
felt_proud = 0.5;
felt_hole_margin = 0.5; // felt hole this much larger than the window on each side

$fn = 48;

// ---- derived -----------------------------------------------------------------
front_t = sensor_total - pcb_t - felt_proud; // skin-face plate under the board
assert(front_t >= 0.6, "skin-face plate too thin: reduce felt_proud or check sensor_total / pcb_t");
pocket_w = sensor_w + 2 * fit;
pocket_h = sensor_h + 2 * fit;
outer_w = pocket_w + 2 * wall;
outer_l = wall + pocket_h + wire_gap + sock_depth + socket_wall;
cx = outer_w / 2;                         // centre line in x
sensor_cy = wall + pocket_h / 2;          // sensor centre in y
win_cx = cx + win_dx;
win_cy = sensor_cy + win_dy;
open_w = win_w + 2 * fit;                 // window opening
open_h = win_h + 2 * fit;
sock_y0 = outer_l - socket_wall - sock_depth; // socket body start (rear end)
sock_zc = (body_h + lid_t) / 2;           // socket centre height
boss_y = sock_y0 + sock_depth / 2;
boss_dx = sock_w / 2 + fit + boss_d / 2;  // bosses either side of the socket body
loop_l = strap_w + 2 * 3;                 // loop length along y

module rounded_rect(w, h, r) {
  offset(r) square([w - 2 * r, h - 2 * r], center = true);
}

module strap_loop(side) { // side = -1 left, +1 right
  x0 = side < 0 ? -(strap_slot + loop_bar) : outer_w;
  translate([x0, sensor_cy - loop_l / 2, 0])
    difference() {
      cube([strap_slot + loop_bar, loop_l, loop_h]);
      // slot next to the body, the elastic goes through it and around the outer bar
      translate([side < 0 ? loop_bar : 0, 3, -1]) cube([strap_slot, strap_w, loop_h + 2]);
    }
}

module curve_skin_face() {
  if (forehead_r > 0)
    intersection() {
      children();
      translate([cx, -50, forehead_r]) rotate([-90, 0, 0])
        cylinder(r = forehead_r, h = outer_l + 100, $fn = 360);
    }
  else
    children();
}

module body() {
  curve_skin_face() difference() {
    union() {
      cube([outer_w, outer_l, body_h]);
      strap_loop(-1);
      strap_loop(1);
    }
    // inner cavity, open toward the lid
    translate([wall, wall, front_t]) cube([pocket_w, outer_l - wall - socket_wall, body_h]);
    // window for the sensor chip
    translate([win_cx, win_cy, -1]) linear_extrude(front_t + 2) rounded_rect(open_w, open_h, 0.5);
    // USB-C panel cutout
    translate([cx, outer_l - socket_wall - 1, sock_zc]) rotate([-90, 0, 0])
      linear_extrude(socket_wall + 2) rounded_rect(cut_w, cut_h, cut_r);
  }
  // cradle under the socket body
  cradle_top = sock_zc - sock_h / 2;
  translate([cx - sock_w / 2, sock_y0, front_t - 0.01]) cube([sock_w, sock_depth, cradle_top - front_t]);
  // screw bosses
  for (s = [-1, 1])
    translate([cx + s * boss_dx, boss_y, front_t - 0.01])
      difference() {
        cylinder(d = boss_d, h = body_h - front_t + 0.01);
        cylinder(d = screw_pilot, h = body_h);
      }
}

// Printed flat side down; the rib presses the socket body onto its cradle.
module lid() {
  rib_h = body_h - (sock_zc + sock_h / 2) + 0.2; // 0.2 mm squeeze on the rubber socket body
  difference() {
    union() {
      cube([outer_w, outer_l, lid_t]);
      translate([cx - sock_w / 2 + 1, sock_y0 + 1, lid_t - 0.01]) cube([sock_w - 2, sock_depth - 2, rib_h]);
    }
    for (s = [-1, 1])
      translate([cx + s * boss_dx, boss_y, -1]) cylinder(d = screw_clear, h = lid_t + 2);
  }
}

// Felt cutting template: the skin face of the body (without the strap loops),
// with the window hole. Export as SVG/DXF and print at 100 %.
module felt() {
  difference() {
    square([outer_w, outer_l]);
    translate([win_cx, win_cy]) rounded_rect(open_w + 2 * felt_hole_margin, open_h + 2 * felt_hole_margin, 0.8);
  }
}

module socket_preview() {
  color("dimgray") translate([cx, outer_l, sock_zc]) rotate([-90, 0, 0]) {
    translate([0, 0, 0]) linear_extrude(1.5) rounded_rect(flange_w, flange_h, 2);
    translate([0, 0, -sock_depth]) linear_extrude(sock_depth) rounded_rect(sock_w, sock_h, 1);
  }
}

if (part == "body") body();
else if (part == "lid") lid();
else if (part == "felt") felt();
else {
  color("black") body();
  color("darkslategray", 0.6) translate([0, 0, body_h + 15]) mirror([0, 0, 1]) translate([0, 0, -lid_t]) lid();
  color("dimgray") translate([0, 0, -10]) linear_extrude(1.5) felt();
  socket_preview();
}
