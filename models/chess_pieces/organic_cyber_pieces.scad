// Organic-cyber series. Beside cybertruck_pieces.scad, not a replacement.
//
// Tělo je hladká křivka (rotate_extrude). Cyber je V-drážka, zářez, pláty
// koruny a kříž. Patka je osmihran jako u druhé řady, ať sedí magnety.
//
// Převis: poloměr smí růst nejvýš o tan(28°) na milimetr výšky.
// Zužování je volné. Drážka má horní stěnu strmou, bez vodorovného dna.
// Výšky a magnety stejné jako u cybertruck řady.

/* [Piece] */
part = "set"; // [set, lineup, pawn, rook, knight, bishop, queen, king]

magnet_diameter = 6.2;
magnet_thickness = 2.2;
magnet_z_offset = 0.8;
sensor_pitch = 6.267;
sensor_axis = 0;

base_d = 28;
royal_d = 34;
base_h = 5;
pawn_h = 45;
body_fn = 8;
magnet_fn = 50;

magnet_pause_z = magnet_z_offset + magnet_thickness;
pocket_x = (magnet_diameter + 1.0) / 2;
quad_r = (magnet_diameter + 1.15) / sqrt(2);
_steel = [0.73, 0.76, 0.80];
_gap = 42;

pawn_pockets   = [[0, 0, 1]];
rook_pockets   = [[0, 0, -1]];
knight_pockets = [[pocket_x, 0, 1]];
bishop_pockets = [[pocket_x, 0, 1], [-pocket_x, 0, -1]];
queen_pockets  = [[quad_r, 0, 1], [-quad_r, 0, 1], [0, quad_r, 1], [0, -quad_r, 1]];
king_pockets   = [[quad_r, 0, -1], [-quad_r, 0, -1], [0, quad_r, -1], [0, -quad_r, -1]];

echo(str("M600 pause Z = ", magnet_pause_z, " mm"));

module disk(d, fn) {
    linear_extrude(height = 0.04)
        circle(d = d, $fn = fn);
}

module frustum(z0, d0, z1, d1, fn = body_fn, rot = 22.5) {
    hull() {
        translate([0, 0, z0])
            rotate([0, 0, rot])
                disk(d0, fn);
        translate([0, 0, z1 - 0.04])
            rotate([0, 0, rot])
                disk(d1, fn);
    }
}

module plinth(d = base_d) {
    frustum(0, d, base_h - 1.2, d);
    frustum(base_h - 1.2, d, base_h, d - 3.4);
}

module foot(ankle_z, ankle_d, d = base_d) {
    plinth(d);
    frustum(base_h, d - 3.4, ankle_z, ankle_d);
}

module magnet_cavities(pockets) {
    for (p = pockets)
        rotate([0, 0, sensor_axis])
            translate([p[0], p[1], magnet_z_offset])
                cylinder(d = magnet_diameter, h = magnet_thickness, $fn = magnet_fn);
}

module with_pockets(pockets) {
    difference() {
        children();
        magnet_cavities(pockets);
    }
}

// [r, z], z roste. Uzavřeno k ose.
module grown(profile) {
    rotate_extrude($fn = 40)
        polygon(concat(
            [for (p = profile) [max(p[0], 0.08), p[1]]],
            [
                [0.08, profile[len(profile) - 1][1]],
                [0.08, profile[0][1]]
            ]
        ));
}

// Horní stěna: 1,15 mm dovnitř na 2,7 mm výšky → 23° od svislice.
module seam(z, r) {
    rotate_extrude($fn = 48)
        polygon([
            [r - 1.15, z],
            [r + 0.55, z - 1.05],
            [r + 0.55, z + 2.7]
        ]);
}

module pawn_shape() {
    difference() {
        union() {
            foot(11, 16.2);
            grown([
                [7.4, 9],
                [6.5, 17],
                [6.3, 24.5],
                [8.5, 30.6],
                [8.6, 32.2],
                [7.5, 33.4],
                [10.3, 40.2],
                [10.1, 41.5],
                [7.3, pawn_h]
            ]);
        }
        seam(21.5, 6.5);
    }
}

module rook_merlon() {
    hull() {
        cube([4.4, 3.4, 0.2], center = true);
        translate([-0.15, -0.7, 6.2])
            cube([3.2, 2.2, 0.2], center = true);
    }
}

module rook_shape() {
    difference() {
        union() {
            foot(9, 20.4);
            grown([
                [9.0, 7.5],
                [7.6, 18],
                [7.6, 32],
                [8.0, 44],
                [8.6, 48.6],
                [8.0, 51.2]
            ]);
            for (a = [0 : 90 : 270])
                rotate([0, 0, a])
                    translate([0, 6.0, 47.6])
                        rook_merlon();
        }
        seam(22, 8.6);
    }
}

module knight_rib(y, z0, z1, hw) {
    h = max(z1 - z0, 0.4);
    c = min(hw * 0.35, h * 0.18);
    // Dolní zkosení je strmé. Stejné dx i dz by bylo 45° a slicer ho chytí.
    translate([0, y, 0])
        hull() {
            translate([-hw + c, 0, z0]) cube(0.06, center = true);
            translate([hw - c, 0, z0]) cube(0.06, center = true);
            translate([-hw, 0, z0 + c * 2.4]) cube(0.06, center = true);
            translate([hw, 0, z0 + c * 2.4]) cube(0.06, center = true);
            translate([-hw, 0, z1 - c]) cube(0.06, center = true);
            translate([hw, 0, z1 - c]) cube(0.06, center = true);
            translate([-hw + c, 0, z1]) cube(0.06, center = true);
            translate([hw - c, 0, z1]) cube(0.06, center = true);
        }
}

module knight_loft(a, b) {
    hull() {
        knight_rib(a[0], a[1], a[2], a[3]);
        knight_rib(b[0], b[1], b[2], b[3]);
    }
}

module knight_column(top_z, top_d) {
    frustum(2.4, 18.2, 7.2, 15.2);
    frustum(7.2, 15.2, top_z, top_d);
}

module knight_ear() {
    hull() {
        translate([0, 5.0, 45.5])
            cube([2.6, 3.4, 2.2], center = true);
        translate([0, 3.2, 52.8])
            cube([1.3, 1.4, 0.55], center = true);
    }
}

module knight_optic(s) {
    hull() {
        translate([s * 4.3, 4.4, 40.2])
            cube([1.5, 2.6, 2.6], center = true);
        translate([s * 6.6, 4.8, 43.8])
            cube([0.4, 1.4, 0.4], center = true);
    }
}

module knight_shape() {
    // Křivka krku, čumák, čelist, hřeben. Spodek čumáku ~26° od svislice.
    union() {
        foot(10, 18.4);
        knight_column(16, 13.6);
        knight_loft([-5.0, 5.8, 26, 5.8], [-1.6, 6.6, 36, 6.6]);
        knight_loft([-1.6, 6.6, 36, 6.6], [2.2, 9.0, 43, 6.6]);
        knight_loft([1.8, 24, 45, 6.0], [5.4, 33, 49.5, 5.1]);
        knight_loft([5.2, 32, 47.6, 4.6], [10.8, 43.2, 47.4, 2.8]);
        knight_loft([2.0, 12, 25, 4.8], [6.4, 21, 30.2, 3.4]);
        knight_loft([6.4, 21, 30.2, 3.4], [10.2, 32.4, 38.2, 2.3]);
        knight_loft([-4.2, 22, 34, 1.25], [0.6, 33, 45.5, 1.05]);
        knight_loft([0.6, 33, 45.5, 1.05], [3.6, 41, 50.4, 0.9]);
        translate([3.3, 0, 0]) knight_ear();
        translate([-3.3, 0, 0]) knight_ear();
        knight_optic(1);
        knight_optic(-1);
    }
}

module bishop_shape() {
    difference() {
        union() {
            foot(12, 16.2);
            grown([
                [7.5, 10],
                [6.4, 22],
                [7.1, 33],
                [8.5, 40.5],
                [5.6, 51],
                [2.9, 59.2]
            ]);
        }
        seam(26, 6.6);
        translate([-0.8, -14, 45])
            cube([1.6, 28, 18]);
    }
}

module queen_petal() {
    // Stopa na límci, ven až vzhůru. 2,4 mm na 14 mm → 10° od svislice.
    hull() {
        cube([2.2, 4.4, 0.2], center = true);
        translate([2.4, 0, 14])
            cube([1.5, 2.0, 0.2], center = true);
        translate([1.1, 0, 18.2])
            cube([1.1, 1.3, 0.2], center = true);
    }
}

module queen_shape() {
    difference() {
        union() {
            foot(15, 18.4, royal_d);
            grown([
                [8.6, 13],
                [7.2, 28],
                [8.2, 40],
                [10.2, 48.2],
                [9.5, 51.2]
            ]);
            grown([
                [2.4, 50.5],
                [5.0, 58],
                [2.3, 70]
            ]);
            for (i = [0 : 5])
                rotate([0, 0, i * 60 + 30])
                    translate([7.4, 0, 47.4])
                        queen_petal();
        }
        seam(30, 7.4);
    }
}

module king_merlon() {
    hull() {
        cube([2.8, 2.8, 0.2], center = true);
        translate([-0.35, 0, 5.4])
            cube([1.6, 1.6, 0.2], center = true);
    }
}

module king_bar(size) {
    ch = 0.6;
    hull() {
        cube([size[0] - 2 * ch, size[1] - 2 * ch, size[2]], center = true);
        cube([size[0], size[1], size[2] - 2 * ch], center = true);
    }
}

module king_arm() {
    // Vnější stěna stojí na koruně. Spodek: 6,1 mm dovnitř na 13,5 mm → 24°.
    hull() {
        translate([8.2, 0, 50.6])
            cube([0.2, 3.8, 0.2], center = true);
        translate([8.2, 0, 66.5])
            cube([0.2, 3.8, 0.2], center = true);
        translate([2.1, 0, 70.4])
            cube([0.2, 3.6, 0.2], center = true);
        translate([2.1, 0, 64.1])
            cube([0.2, 3.6, 0.2], center = true);
    }
}

module king_shape() {
    difference() {
        union() {
            foot(16, 20.4, royal_d);
            grown([
                [9.4, 14],
                [7.8, 30],
                [8.8, 42],
                [10.5, 50.2],
                [9.6, 53]
            ]);
            for (i = [0 : 7])
                rotate([0, 0, i * 45 + 22.5])
                    translate([7.2, 0, 49.2])
                        king_merlon();
            translate([0, 0, 66])
                king_bar([4.4, 4.4, 28]);
            king_arm();
            mirror([1, 0, 0])
                king_arm();
        }
        seam(32, 8.0);
    }
}

module pawn() { with_pockets(pawn_pockets) pawn_shape(); }
module rook() { with_pockets(rook_pockets) rook_shape(); }
module knight() { with_pockets(knight_pockets) knight_shape(); }
module bishop() { with_pockets(bishop_pockets) bishop_shape(); }
module queen() { with_pockets(queen_pockets) queen_shape(); }
module king() { with_pockets(king_pockets) king_shape(); }

module lineup() {
    translate([0 * _gap, 0, 0]) pawn();
    translate([1 * _gap, 0, 0]) rook();
    translate([2 * _gap, 0, 0]) knight();
    translate([3 * _gap, 0, 0]) bishop();
    translate([4 * _gap, 0, 0]) queen();
    translate([5 * _gap, 0, 0]) king();
}

module showroom() {
    color(_steel) {
        translate([0 * _gap, 0, 0]) pawn();
        translate([1 * _gap, 0, 0]) rook();
        translate([2 * _gap, 0, 0]) rotate([0, 0, 180]) knight();
        translate([3 * _gap, 0, 0]) bishop();
        translate([4 * _gap, 0, 0]) queen();
        translate([5 * _gap, 0, 0]) king();
    }
    color([0.11, 0.12, 0.14])
        translate([-26, -28, -0.7])
            cube([5 * _gap + 52, 56, 0.7]);
}

if (part == "set")
    showroom();
else if (part == "lineup")
    lineup();
else if (part == "pawn")
    color(_steel) pawn();
else if (part == "rook")
    color(_steel) rook();
else if (part == "knight")
    color(_steel) knight();
else if (part == "bishop")
    color(_steel) bishop();
else if (part == "queen")
    color(_steel) queen();
else if (part == "king")
    color(_steel) king();
