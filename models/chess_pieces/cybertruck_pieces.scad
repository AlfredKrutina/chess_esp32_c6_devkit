// Cybertruck chess set — low-poly, print-in-place magnet code.
//
// Tisk základnou dolů, bez podpěr. Převis nejvýš 40° od svislice:
// při rozšiřování musí být výška >= (d1 - d0) / 2 / tan(40°).
// Zužování a svislá stěna jsou v pořádku. Žádný objem nezačíná ve vzduchu.
//
// Stabilita: pěšec až střelec mají podstavu 28 mm. Dáma a král 34 mm —
// král 80 mm na podstavě 28 mm má mez překlopení jen kolem 24°.
// Hmotnost je ve patce, dřík na úchop má napříč alespoň 13 mm,
// hroty jsou tupé (nejméně 1,6 mm), ať figurka nebodá a neutrhne se.
//
// Hall V2: dva analogové senzory na pole, rozteč sensor_pitch, citlivá osa kolmo
// k desce. Senzor je sensor_gap mm POD horní hranou hrací plochy. Figurka stojí
// na té hraně, takže každý milimetr plastu pod magnetem je milimetr navíc ve
// vzduchové mezeře. Proto je dno dutiny jen 0.8 mm (čtyři vrstvy 0.2 mm).
//
// Slicer: pauza (M600) ve výšce
//   Z = magnet_z_offset + magnet_thickness
// Všechny dutiny jedné figurky jsou v téže výšce — na pauze se vloží všechny
// její magnety najednou, pak tisk strop zavře.
//
// Polarita (značka na magnetu, ta strana dolů, k desce):
//   +1 sever dolů  — v náhledu červená
//   -1 jih dolů    — v náhledu modrá
//
// Kódy jsou zvolené tak, aby se mraky (B0, B1) nepřekrývaly při mezeře 6–8 mm:
//   pěšec   1× střed, sever     libovolné otočení
//   věž     1× střed, jih       libovolné otočení
//   dáma    4× na kříži, sever  libovolné otočení
//   král    4× na kříži, jih    libovolné otočení
//   jezdec  1× vpravo, sever    čumák dopředu nebo dozadu; bokem splyne s pěšcem
//   střelec 1× vpravo sever + 1× vlevo jih
//           čelem dopředu nebo dozadu; otočení o 90° se vyruší a pole je prázdné
//
// sensor_axis = 0  pár leží vlevo-vpravo (osa X figurky)
// sensor_axis = 90 pár leží dopředu-dozadu — celý kód se otočí
//
// Část: set | lineup | pawn | rook | knight | knight_low | knight_sketch | bishop | queen | king | section | pattern
// knight_sketch je plochý výtlaček profilu, schovaný v archive/knight_sketch.stl.
// knight_low je jezdec pro Blender. Jedna nakloněná hlava, úzký pas, bez uší,
// huby a očí. Čumák přečnívá — netisknout bez podpěr. Tisknutelný jezdec je part knight.

/* [Piece] */
part = "set"; // [set, pawn, rook, knight, knight_low, knight_sketch, bishop, queen, king, section, pattern]

/* [Magnet — kalibrace vůči Hall senzorům] */
magnet_diameter = 6.2;  // disk 6 mm + 0.2 mm vůle
magnet_thickness = 2.2; // výška 2 mm + 0.2 mm vůle
magnet_z_offset = 0.8;  // plast pod magnetem; senzor je až pod deskou, proto co nejníž

sensor_pitch = 6.267; // mm, střed–střed páru na jednom poli
sensor_gap = 7;       // mm, horní hrana desky → Hall senzor (jen pro náhled)
sensor_axis = 0;      // [0, 90]

/* [Base] */
base_d = 28;
royal_d = 34; // dáma a král: širší patka, ať se nepřeklopí
base_h = 5;
pawn_h = 45;
body_fn = 8;     // podstava a dřík: 8úhelník
head_fn = 6;     // hlavy: 6úhelník
magnet_fn = 50;  // jen dutina magnetu

/* [Hidden] */
magnet_pause_z = magnet_z_offset + magnet_thickness;
// Dva 6.2mm otvory vedle sebe potřebují mezeru; senzory jsou 6.267 mm,
// takže dutina sedí 0.5 mm mimo střed senzoru a mezi otvory zbývá 1 mm stěny.
pocket_x = (magnet_diameter + 1.0) / 2;
quad_r = (magnet_diameter + 1.15) / sqrt(2);
_steel = [0.73, 0.76, 0.80];
_north = [0.82, 0.14, 0.12];
_south = [0.18, 0.38, 0.82];
_gap = 42;

pawn_pockets   = [[0, 0, 1]];
rook_pockets   = [[0, 0, -1]];
knight_pockets = [[pocket_x, 0, 1]];
bishop_pockets = [[pocket_x, 0, 1], [-pocket_x, 0, -1]];
queen_pockets  = [[quad_r, 0, 1], [-quad_r, 0, 1], [0, quad_r, 1], [0, -quad_r, 1]];
king_pockets   = [[quad_r, 0, -1], [-quad_r, 0, -1], [0, quad_r, -1], [0, -quad_r, -1]];

echo(str("M600 pause Z = ", magnet_pause_z, " mm"));
echo(str("pocket offset from sensor mm ", pocket_x - sensor_pitch / 2));

module disk(d, fn) {
    linear_extrude(height = 0.04)
        circle(d = d, $fn = fn);
}

// Zkosený komolý jehlan. rot = 22.5° otočí osmiúhelník plochou stěnou dopředu.
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

// Patka nad magnety. ankle_d musí být <= d - 3.4, jinak vznikne převis.
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

module show_magnets(pockets) {
    for (p = pockets)
        color(p[2] > 0 ? _north : _south)
            rotate([0, 0, sensor_axis])
                translate([p[0], p[1], magnet_z_offset + 0.05])
                    cylinder(d = magnet_diameter - 0.2, h = magnet_thickness - 0.1, $fn = magnet_fn);
}

module chamfer_slab(depth, ch) {
    hull() {
        linear_extrude(0.04)
            offset(delta = -ch)
                children();
        translate([0, 0, ch])
            linear_extrude(0.04)
                children();
        translate([0, 0, depth - ch - 0.04])
            linear_extrude(0.04)
                children();
        translate([0, 0, depth - 0.04])
            linear_extrude(0.04)
                offset(delta = -ch)
                    children();
    }
}

// ---------------------------------------------------------------------------
// Pawn — 45 mm. Krátký dřík, límec, šestihranná hlava širší než vysoká.
// To je pěšec: hlava oddělená od krku, tupý vršek, ne špička a ne přilba.
// Límec: poloměr +2,2 mm na 4,6 mm → 26° od svislice.
// Hlava: poloměr +3,8 mm na 7,0 mm → 28° od svislice.
// ---------------------------------------------------------------------------
module pawn_shape() {
    foot(11, 16.2);
    frustum(11, 16.2, 20, 13.6);
    frustum(20, 13.6, 27.2, 13.2);
    frustum(27.2, 13.2, 31.8, 17.6);
    frustum(31.8, 17.6, 33.2, 17.6);
    frustum(33.2, 17.6, 34.6, 15.4);
    // Šestihran začíná uvnitř osmihranu. Stejný průměr by na vrcholech
    // udělal vodorovnou římsu, a tu slicer podepře.
    frustum(34.2, 13.4, 41.2, 21.0, head_fn, 0);
    frustum(41.2, 21.0, 42.2, 21.0, head_fn, 0);
    frustum(42.2, 21.0, pawn_h, 15.0, head_fn, 0);
}

module pawn() {
    with_pockets(pawn_pockets)
        pawn_shape();
}

// ---------------------------------------------------------------------------
// Rook — hranolová věž, čtyři cimbuří, zkosená koruna.
// ---------------------------------------------------------------------------
module rook_merlon() {
    // Úzký zub, mezi nimi je mezera — to je ta věž, ne čtyři boule.
    hull() {
        cube([4.4, 5.8, 0.04], center = true);
        translate([0, -0.45, 11.0])
            cube([2.8, 3.4, 0.04], center = true);
    }
}

module rook_shape() {
    union() {
        foot(9, 21.2);
        frustum(9, 21.2, 22, 18.2);
        // Opasek: zářez a návrat, návrat je 30° od svislice.
        frustum(22, 18.2, 23.3, 16.4);
        frustum(23.3, 16.4, 25.6, 16.4);
        frustum(25.6, 16.4, 27.0, 18.2);
        frustum(27.0, 18.2, 37.5, 17.4);
        frustum(37.5, 17.4, 42.2, 23.2);
        frustum(42.2, 23.2, 45.4, 23.2);
        frustum(45.4, 23.2, 46.6, 20.6);
        for (a = [45 : 90 : 315])
            rotate([0, 0, a])
                translate([0, 6.2, 44.6])
                    rook_merlon();
    }
}

module rook() {
    with_pockets(rook_pockets)
        rook_shape();
}

// ---------------------------------------------------------------------------
// Knight — jeden konkávní profil. Zkosení přes hull() by ho zase srovnalo
// do desky, proto je tloušťka bez chamfer_slab.
// ---------------------------------------------------------------------------
module knight_panel(points, depth, chamfer = 0) {
    translate([0, 0, 3.5])
        rotate([0, 0, 90])
            rotate([90, 0, 0])
                translate([0, 0, -depth / 2])
                    knight_panel_extrude(points, depth, chamfer);
}

module knight_panel_extrude(points, depth, chamfer) {
    if (chamfer > 0)
        chamfer_slab(depth, chamfer)
            polygon(points);
    else
        linear_extrude(depth)
            polygon(points);
}

module knight_sketch_shape() {
    // Plochý profil. Nechat. Nový jezdec je knight_shape.
    difference() {
        union() {
            foot(10, 18.4);
            knight_panel([
                [-5.4, 0.5], [4.8, 0.5], [5.4, 16.5], [3.0, 22.5],
                [4.0, 26.0], [11.8, 41.0], [10.8, 45.2], [5.6, 48.0],
                [2.8, 51.0], [0.6, 51.0], [2.4, 46.0],
                [-1.0, 41.5], [-4.2, 30.0], [-5.4, 12.0]
            ], 15.0, 0);
        }
        knight_panel([
            [6.6, 28.0], [10.2, 33.8], [8.6, 40.0], [5.6, 33.5]
        ], 20.0, 0);
    }
}

module knight_sketch() {
    with_pockets(knight_pockets)
        knight_sketch_shape();
}

// Řez s rovnou horní a spodní hranou. Hrot by z krku udělal krystal.
module knight_rib(y, z0, z1, hw) {
    h = max(z1 - z0, 0.4);
    c = min(1.8, h * 0.18);
    translate([0, y, 0])
        hull() {
            translate([-hw + c, 0, z0]) cube(0.06, center = true);
            translate([hw - c, 0, z0]) cube(0.06, center = true);
            translate([-hw, 0, z0 + c]) cube(0.06, center = true);
            translate([hw, 0, z0 + c]) cube(0.06, center = true);
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

// Loft krku je plát silný jen pár milimetrů. Bez sloupu se ulomí v prvním
// spoji s podstavou. Sloup je zanořený do patky, průřez neklesne pod ~14 mm.
module knight_column(top_z, top_d) {
    frustum(2.4, 18.2, 7.2, 15.2);
    frustum(7.2, 15.2, top_z, top_d);
}

module knight_ear() {
    // Krátké, sklopené dozadu. Pata je širší než hrot, ať se ucho
    // neutrhne. Vysoké antény zepředu čtou jako cimbuří.
    hull() {
        translate([0, 5.0, 44.6])
            cube([3.8, 4.6, 2.8], center = true);
        translate([0, 3.6, 52.2])
            cube([1.8, 2.2, 0.8], center = true);
    }
}

module knight_optic(s) {
    // Šupina na líci. Vnější hrana je výš, spodek ~27° od svislice.
    hull() {
        translate([s * 4.4, 4.3, 40.0])
            cube([1.4, 2.8, 2.4], center = true);
        translate([s * 6.7, 4.7, 43.6])
            cube([0.45, 1.5, 0.45], center = true);
    }
}

module knight_shape() {
    // Plný krk, lebka, čumák užší než líce, čelist pod otevřenou hubou.
    // Hříva je jeden hřeben. Oko je šupina, ne díra s vodorovným dnem.
    // Spodek čumáku ~26° od svislice.
    union() {
        foot(10, 18.4);
        knight_column(16, 13.6);
        knight_loft([-4.8, 5.5, 30, 6.4], [1.2, 7.0, 41, 7.0]);
        knight_loft([1.2, 7.0, 41, 7.0], [4.2, 12, 46, 6.2]);
        knight_loft([2.2, 24, 46, 6.0], [6.0, 33, 50, 5.2]);
        knight_loft([5.2, 30, 48, 5.0], [11.0, 42.2, 47.6, 3.0]);
        knight_loft([2.0, 12, 26, 5.2], [6.6, 20, 29.5, 3.6]);
        knight_loft([6.6, 20, 29.5, 3.6], [10.3, 31.5, 37.2, 2.5]);
        knight_loft([-4.2, 22, 35, 1.35], [0.6, 32, 45.5, 1.15]);
        knight_loft([0.6, 32, 45.5, 1.15], [3.8, 40, 50.6, 1.0]);
        translate([3.4, 0, 0]) knight_ear();
        translate([-3.4, 0, 0]) knight_ear();
        knight_optic(1);
        knight_optic(-1);
    }
}

module knight() {
    with_pockets(knight_pockets)
        knight_shape();
}

// Jezdec pro Blender. Pas se stáhne, hlava je jeden klín nakloněný dopředu.
// Žádné uši, huba ani oči. Čumák přečnívá — netisknout bez podpěr.
// Krk vyrůstá ze stejného osmiúhelníku jako patka. Užší sloupek na něm
// nechával rohy patky trčet vedle stěny.
module knight_low_shape() {
    ankle_z = 9.6;
    ankle_d = 16.8;
    union() {
        foot(ankle_z, ankle_d);
        // Same octagon as the foot, then the taper continues. A vertical
        // peg on the flat top left the foot's corners sticking out.
        hull() {
            translate([0, 0, ankle_z - 0.35])
                rotate([0, 0, 22.5])
                    cylinder(h = 0.5, d = ankle_d, $fn = body_fn);
            translate([0, 0, 15.2])
                rotate([0, 0, 22.5])
                    cylinder(h = 0.4, d = 11.4, $fn = body_fn);
            knight_rib(-5.2, 18, 26, 5.6);
            knight_rib(4.8, 18, 26, 5.6);
            knight_rib(-3.0, 34, 52, 7.6);
            knight_rib(12.4, 33, 46, 6.2);
        }
    }
}

module knight_low() {
    with_pockets(knight_pockets)
        knight_low_shape();
}

// ---------------------------------------------------------------------------
// Bishop — štíhlý dřík, šestihranná mitra se zářezem.
// ---------------------------------------------------------------------------
module bishop_shape() {
    difference() {
        union() {
            foot(12, 16.4);
            frustum(12, 16.4, 28, 13.8);
            frustum(28, 13.8, 34.5, 17.6);
            // Mitra je třetina výšky, ne špička na lahvi.
            translate([0, 0, 34.0])
                rotate([0, 0, 30])
                    frustum(0, 16.2, 25.5, 7.6, head_fn, 0);
        }
        // Zářez otevřený nahoru, přes celou přední stěnu.
        // Hrot 7,6 mm, zářez 1,8 mm, po stranách zbývá 2,9 mm.
        translate([-0.9, -16, 42])
            cube([1.8, 32, 22]);
    }
}

module bishop() {
    with_pockets(bishop_pockets)
        bishop_shape();
}

// ---------------------------------------------------------------------------
// Queen — šestihranný krystal a šest jehlanů. Výška 70 mm.
// ---------------------------------------------------------------------------
module queen_spike() {
    // Korunka. Stopa leží na límci a je širší než hrot, ať se hrot
    // neutrhne v jedné vrstvě. Ven se láme až vzhůru.
    hull() {
        cube([5.2, 5.2, 0.04], center = true);
        translate([1.6, 0, 15.2])
            cube([2.8, 2.8, 0.04], center = true);
    }
}

module queen_shape() {
    union() {
        foot(15, 18.4, royal_d);
        frustum(15, 18.4, 36, 14.4);
        frustum(36, 14.4, 44, 20.4);
        frustum(44, 20.4, 49.2, 20.4);
        frustum(49.2, 20.4, 50.8, 17.2);
        translate([0, 0, 56])
            rotate([0, 0, 30]) {
                // Klenot je menší než hroty korunky, ať dáma není druhý střelec.
                frustum(-6.5, 5.0, 0, 12.4, head_fn, 0);
                frustum(0, 12.4, 14.0, 3.2, head_fn, 0);
            }
        for (i = [0 : 5])
            rotate([0, 0, i * 60 + 30])
                translate([7.0, 0, 48.2])
                    queen_spike();
    }
}

module queen() {
    with_pockets(queen_pockets)
        queen_shape();
}

// ---------------------------------------------------------------------------
// King — nejvyšší, osm cimbuří a zkosený kříž. Výška 80 mm.
// ---------------------------------------------------------------------------
module king_merlon() {
    hull() {
        cube([3.8, 3.8, 0.04], center = true);
        translate([-0.3, 0, 6.0])
            cube([2.2, 2.2, 0.04], center = true);
    }
}

module king_bar(size) {
    ch = 0.7;
    hull() {
        cube([size[0] - 2 * ch, size[1] - 2 * ch, size[2]], center = true);
        cube([size[0], size[1], size[2] - 2 * ch], center = true);
    }
}

module king_arm() {
    // Rameno je u dříku tlustší než na konci. Spodní hrana stoupá k dříku
    // (~62° od vodorovné, 28° od svislice), takže pod ním není strop.
    hull() {
        translate([7.4, 0, 49.8])
            cube([0.4, 4.4, 0.4], center = true);
        translate([7.4, 0, 67.2])
            cube([0.4, 4.4, 0.4], center = true);
        translate([1.3, 0, 71.2])
            cube([0.4, 5.8, 0.4], center = true);
        translate([1.3, 0, 61.0])
            cube([0.4, 5.8, 0.4], center = true);
    }
}

module king_cross_root() {
    // Dřík 4,6 mm se v koruně urve. V ústí koruny má stopu ~7,5 mm
    // a během pár milimetrů se stáhne do dříku, ať z toho není druhá věž.
    hull() {
        translate([0, 0, 48.8])
            cube([9.4, 9.4, 0.2], center = true);
        translate([0, 0, 53.8])
            cube([5.4, 5.4, 0.2], center = true);
    }
}

module king_shape() {
    union() {
        foot(16, 21.2, royal_d);
        frustum(16, 21.2, 34, 15.6);
        frustum(34, 15.6, 42, 20.8);
        frustum(42, 20.8, 49.4, 20.8);
        frustum(49.4, 20.8, 51.2, 17.6);
        for (i = [0 : 7])
            rotate([0, 0, i * 45 + 22.5])
                translate([6.6, 0, 47.6])
                    king_merlon();
        // Dřík kříže ční nad ramena, ať je to kříž a ne komín.
        // Kořen je široký, dřík nad ním zůstává úzký.
        king_cross_root();
        translate([0, 0, 66.4])
            king_bar([5.4, 5.4, 27.2]);
        king_arm();
        mirror([1, 0, 0])
            king_arm();
    }
}

module king() {
    with_pockets(king_pockets)
        king_shape();
}

// ---------------------------------------------------------------------------
// Náhledy
// ---------------------------------------------------------------------------
module pawn_section() {
    color(_steel)
        difference() {
            pawn();
            translate([-40, 0, -12])
                cube([80, 40, 120]);
        }
    difference() {
        show_magnets(pawn_pockets);
        translate([-20, 0, -1])
            cube([40, 20, 12]);
    }
    // Horní plošky senzorů v rovině z = -sensor_gap.
    color([0.15, 0.55, 0.28])
        for (s = [-1, 1])
            translate([s * sensor_pitch / 2 - 1.2, -1.6, -sensor_gap - 0.9])
                cube([2.4, 3.2, 0.9]);
}

module pattern_base(pockets) {
    difference() {
        plinth();
        translate([-22, -22, magnet_z_offset + magnet_thickness * 0.55])
            cube([44, 44, 20]);
    }
    for (p = pockets)
        color(p[2] > 0 ? _north : _south)
            rotate([0, 0, sensor_axis])
                translate([p[0], p[1], magnet_z_offset])
                    cylinder(
                        d = magnet_diameter - 0.2,
                        h = magnet_thickness * 0.55,
                        $fn = magnet_fn
                    );
}

module pattern_card() {
    translate([0 * _gap, 0, 0]) pattern_base(pawn_pockets);
    translate([1 * _gap, 0, 0]) pattern_base(rook_pockets);
    translate([2 * _gap, 0, 0]) pattern_base(knight_pockets);
    translate([3 * _gap, 0, 0]) pattern_base(bishop_pockets);
    translate([4 * _gap, 0, 0]) pattern_base(queen_pockets);
    translate([5 * _gap, 0, 0]) pattern_base(king_pockets);
}

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
else if (part == "knight_low")
    color(_steel) knight_low();
else if (part == "knight_sketch")
    color(_steel) knight_sketch();
else if (part == "bishop")
    color(_steel) bishop();
else if (part == "queen")
    color(_steel) queen();
else if (part == "king")
    color(_steel) king();
else if (part == "section")
    pawn_section();
else if (part == "pattern")
    pattern_card();
