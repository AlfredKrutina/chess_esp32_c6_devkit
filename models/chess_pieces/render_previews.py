"""Render Cybertruck piece previews. Local tool, not part of the firmware build."""
import subprocess
import sys
from pathlib import Path

OSC = r"C:\Program Files\OpenSCAD\openscad.exe"
ROOT = Path(__file__).resolve().parent
SRC = ROOT / "cybertruck_pieces.scad"
OUT = ROOT / "preview"
OUT.mkdir(exist_ok=True)

SHOTS = {
    "pawn": ["--projection=perspective", "--imgsize=1000,1200", "--autocenter", "--viewall"],
    "rook": ["--projection=perspective", "--imgsize=1000,1200", "--autocenter", "--viewall"],
    "knight": ["--projection=perspective", "--imgsize=1000,1200", "--camera=180,160,90,0,4,28"],
    "bishop": ["--projection=perspective", "--imgsize=1000,1200", "--autocenter", "--viewall"],
    "queen": ["--projection=perspective", "--imgsize=1000,1200", "--autocenter", "--viewall"],
    "king": ["--projection=perspective", "--imgsize=1000,1200", "--autocenter", "--viewall"],
    "section": [
        "--projection=perspective",
        "--imgsize=1200,900",
        "--camera=42,110,2,0,-1,-1",
    ],
    "pattern": [
        "--projection=ortho",
        "--imgsize=1800,520",
        "--camera=105,0,280,105,0,1",
    ],
    "set": [
        "--projection=perspective",
        "--imgsize=1920,1080",
        "--camera=168,-450,188,108,0,28",
    ],
}


def render(part: str, extra: list[str]) -> None:
    png = OUT / f"{part}.png"
    cmd = [
        OSC,
        "-o",
        str(png),
        "-D",
        f'part="{part}"',
        "--preview",
        "--colorscheme=Tomorrow",
        *extra,
        str(SRC),
    ]
    print("RUN", part)
    result = subprocess.run(cmd, capture_output=True, text=True)
    err = (result.stderr or "") + (result.stdout or "")
    if "WARNING" in err or "ERROR" in err or result.returncode != 0:
        print(err)
    print(" ->", png.name, "rc", result.returncode)


def export_stl() -> None:
    stl_dir = ROOT / "stl"
    stl_dir.mkdir(exist_ok=True)
    for part in ("pawn", "rook", "knight", "bishop", "queen", "king"):
        stl = stl_dir / f"{part}.stl"
        cmd = [
            OSC,
            "-o",
            str(stl),
            "-D",
            f'part="{part}"',
            "--export-format=binstl",
            str(SRC),
        ]
        print("STL", part)
        result = subprocess.run(cmd, capture_output=True, text=True)
        err = (result.stderr or "") + (result.stdout or "")
        if "WARNING" in err or "ERROR" in err or result.returncode != 0:
            print(err)
        print(" ->", stl.name, "rc", result.returncode)
    stl = stl_dir / "set.stl"
    cmd = [
        OSC,
        "-o",
        str(stl),
        "-D",
        'part="lineup"',
        "--export-format=binstl",
        str(SRC),
    ]
    print("STL set")
    result = subprocess.run(cmd, capture_output=True, text=True)
    err = (result.stderr or "") + (result.stdout or "")
    if "WARNING" in err or "ERROR" in err or result.returncode != 0:
        print(err)
    print(" ->", stl.name, "rc", result.returncode)


if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "stl":
        export_stl()
    else:
        for name, extra in SHOTS.items():
            render(name, extra)
        render(
            "knight",
            ["--imgsize=900,1200", "--projection=ortho", "--camera=260,4,30,0,4,30"],
        )
        (OUT / "knight.png").replace(OUT / "knight_side.png")
        render("knight", SHOTS["knight"])
