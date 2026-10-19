# One STEP assembly of all six discs. Units: millimetres.
# The bought steel disc, diameter 11 mm and thickness 1 mm, sits under
# each magnet with a 6 mm gap. z = 0 is the board side of the plate.
# Hall plane is 5 mm below that face. No air solid.

from pathlib import Path

import cadquery as cq

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "docs" / "simulace-magnetu" / "simscale"

PLATE_D = 11.0
PLATE_H = 1.0
MAGNET_GAP = 6.0
SENSOR_BELOW = 5.0
SENSOR_X = 6.267 / 2.0
PITCH = 35.0

# name, diameter, height, remanence tesla
PIECES = [
    ("pesec", 10.0, 1.0, 1.30),
    ("jezdec", 10.0, 2.0, 1.30),
    ("strelec", 10.0, 3.0, 1.30),
    ("vez", 10.0, 5.0, 1.24),
    ("dama", 10.0, 8.0, 1.30),
    ("kral", 12.0, 6.0, 1.30),
]


def disc(x, z0, diameter, height):
    return cq.Workplane("XY").circle(diameter / 2.0).extrude(height).translate((x, 0.0, z0))


def pack(pieces, pitch):
    magnets = []
    plates = []
    for i, (name, diameter, height, _br) in enumerate(pieces):
        x = i * pitch
        plates.append((name, disc(x, 0.0, PLATE_D, PLATE_H)))
        magnets.append((name, disc(x, PLATE_H + MAGNET_GAP, diameter, height)))
    return magnets, plates


def export(path, magnets, plates):
    assembly = cq.Assembly(name="magnety")
    for name, solid in magnets:
        assembly.add(solid, name=f"{name}_magnet", color=cq.Color(0.76, 0.22, 0.16))
    for name, solid in plates:
        assembly.add(solid, name=f"{name}_plech", color=cq.Color(0.35, 0.38, 0.42))
    assembly.save(str(path), "STEP")


def write_probes(path, pieces, pitch):
    lines = ["body,x_mm,y_mm,z_mm,Br_T"]
    for i, (name, _d, _h, br) in enumerate(pieces):
        x = i * pitch
        lines.append(f"{name}_minus,{x - SENSOR_X:.4f},0,{-SENSOR_BELOW:.1f},{br:.2f}")
        lines.append(f"{name}_plus,{x + SENSOR_X:.4f},0,{-SENSOR_BELOW:.1f},{br:.2f}")
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    magnets, plates = pack(PIECES, PITCH)
    export(OUT / "vsechny.step", magnets, plates)
    write_probes(OUT / "sondy.csv", PIECES, PITCH)
    print("vsechny", (OUT / "vsechny.step").stat().st_size)


if __name__ == "__main__":
    main()
