"""Previews and STLs for the organic-cyber series. Not part of the firmware build."""
import subprocess
import sys
from pathlib import Path

OSC = r"C:\Program Files\OpenSCAD\openscad.exe"
ROOT = Path(__file__).resolve().parent
SRC = ROOT / "organic_cyber_pieces.scad"
OUT = ROOT / "preview" / "organic"
STL = ROOT / "stl" / "organic"

SHOTS = {
    "pawn": ["--projection=perspective", "--imgsize=1000,1200", "--autocenter", "--viewall"],
    "rook": ["--projection=perspective", "--imgsize=1000,1200", "--autocenter", "--viewall"],
    "knight": ["--projection=perspective", "--imgsize=1000,1200", "--camera=160,200,100,0,4,32"],
    "bishop": ["--projection=perspective", "--imgsize=1000,1200", "--autocenter", "--viewall"],
    "queen": ["--projection=perspective", "--imgsize=1000,1200", "--autocenter", "--viewall"],
    "king": ["--projection=perspective", "--imgsize=1000,1200", "--autocenter", "--viewall"],
    "set": ["--projection=perspective", "--imgsize=1920,1080", "--camera=168,-450,188,108,0,28"],
}


def run(cmd):
    result = subprocess.run(cmd, capture_output=True, text=True)
    err = (result.stderr or "") + (result.stdout or "")
    bad = [ln for ln in err.splitlines() if "ERROR" in ln or "WARNING" in ln]
    if bad or result.returncode:
        print("\n".join(bad[-8:]))
    return result.returncode


def render(part, extra):
    OUT.mkdir(parents=True, exist_ok=True)
    png = OUT / f"{part}.png"
    rc = run([
        OSC, "-o", str(png), "-D", f'part="{part}"',
        "--render", "--colorscheme=Tomorrow", *extra, str(SRC),
    ])
    print(png.name, rc)


def export_stl():
    STL.mkdir(parents=True, exist_ok=True)
    for part in ("pawn", "rook", "knight", "bishop", "queen", "king"):
        rc = run([
            OSC, "-o", str(STL / f"{part}.stl"), "-D", f'part="{part}"',
            "--export-format=binstl", str(SRC),
        ])
        print(part, rc)
    rc = run([
        OSC, "-o", str(STL / "set.stl"), "-D", 'part="lineup"',
        "--export-format=binstl", str(SRC),
    ])
    print("set", rc)


if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "stl":
        export_stl()
    else:
        for name, extra in SHOTS.items():
            render(name, extra)
        render("knight", ["--imgsize=900,1200", "--projection=ortho", "--camera=260,6,32,0,6,32"])
        (OUT / "knight.png").replace(OUT / "knight_side.png")
        render("knight", SHOTS["knight"])
