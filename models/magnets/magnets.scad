// Purchased discs and the steel keeper. Geometry only.
// Order 2026-10-18, unimagnet. See tools/magnet_sila.md.
//
// Field belongs in FEMM 4.2 (axisymmetric magnetostatics), not in this file.
//
// Stack, playing surface side at z = 0, keeper on the back (away from the board):
//   magnet disc
//   3M 468MP adhesive, centered, catalog thickness 0.13 mm
//   steel keeper OP-11-01-Zn, diameter 11 mm, thickness 1 mm
//
// The keeper is wider than every disc except the king, and the king disc is
// wider than the keeper. part = "seat" is the counterbore the piece still lacks.

/* [Part] */
part = "lineup"; // [lineup, magnets, pawn, knight, bishop, rook, queen, king, plate, sticker, stack, seat]
piece = "pawn"; // [pawn, knight, bishop, rook, queen, king]

plate_d = 11;
plate_h = 1;
sticker_d = 8;
sticker_h = 0.13; // 3M 468MP, adhesive only, liner excluded
fn = 64;
pitch = 22;

pawn_magnet   = [10, 1];
knight_magnet = [10, 2];
bishop_magnet = [10, 3];
rook_magnet   = [10, 5];
queen_magnet  = [10, 8];
king_magnet   = [12, 6];

function spec_for(name) =
    name == "pawn"   ? pawn_magnet :
    name == "knight" ? knight_magnet :
    name == "bishop" ? bishop_magnet :
    name == "rook"   ? rook_magnet :
    name == "queen"  ? queen_magnet :
    king_magnet;

module magnet_disc(spec) {
    color([0.72, 0.76, 0.80])
        cylinder(d = spec[0], h = spec[1], $fn = fn);
}

module steel_plate() {
    color([0.45, 0.48, 0.52])
        cylinder(d = plate_d, h = plate_h, $fn = fn);
}

module adhesive() {
    color([0.85, 0.72, 0.25])
        cylinder(d = sticker_d, h = sticker_h, $fn = fn);
}

// Bonded stack. Shoulder of a diameter-10 disc is 0.5 mm under the keeper rim.
module stack(spec) {
    magnet_disc(spec);
    translate([0, 0, spec[1]])
        adhesive();
    translate([0, 0, spec[1] + sticker_h])
        steel_plate();
}

// Volume to cut into the piece, sitting on the 1.2 mm floor (floor not included).
// Diameter-10 discs: press-fit bore, then an 11 mm counterbore for the keeper.
// King: one 12 mm bore. The 11 mm keeper is loose by 0.5 mm radially.
module seat(spec) {
    bore_d = max(spec[0], plate_d);
    color([0.25, 0.55, 0.35], 0.45)
        cylinder(d = bore_d, h = spec[1] + sticker_h + plate_h, $fn = fn);
    if (spec[0] < plate_d)
        color([0.20, 0.45, 0.30], 0.45)
            cylinder(d = spec[0], h = spec[1], $fn = fn);
}

module row(which) {
    names = ["pawn", "knight", "bishop", "rook", "queen", "king"];
    for (i = [0:5]) {
        s = spec_for(names[i]);
        translate([i * pitch, 0, 0]) {
            if (which == "magnet") magnet_disc(s);
            else if (which == "stack") stack(s);
            else seat(s);
        }
    }
}

module lineup() {
    row("magnet");
    translate([0, pitch, 0])
        row("stack");
    translate([0, 2 * pitch, 0])
        row("seat");
}

s = spec_for(piece);

if (part == "lineup") lineup();
else if (part == "magnets") row("magnet");
else if (part == "pawn") magnet_disc(pawn_magnet);
else if (part == "knight") magnet_disc(knight_magnet);
else if (part == "bishop") magnet_disc(bishop_magnet);
else if (part == "rook") magnet_disc(rook_magnet);
else if (part == "queen") magnet_disc(queen_magnet);
else if (part == "king") magnet_disc(king_magnet);
else if (part == "plate") steel_plate();
else if (part == "sticker") adhesive();
else if (part == "stack") stack(s);
else if (part == "seat") seat(s);
