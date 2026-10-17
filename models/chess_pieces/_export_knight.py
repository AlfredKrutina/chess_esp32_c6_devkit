import subprocess
from pathlib import Path

OSC = r"C:\Program Files\OpenSCAD\openscad.exe"
SRC = Path(__file__).resolve().parent / "cybertruck_pieces.scad"
ROOT = SRC.parent

jobs = [
    (ROOT / "stl" / "knight.stl", ["-D", 'part="knight"', "--export-format=binstl"]),
    (ROOT / "stl" / "set.stl", ["-D", 'part="lineup"', "--export-format=binstl"]),
    (
        ROOT / "preview" / "knight_side.png",
        [
            "-D", 'part="knight"', "--preview", "--colorscheme=Tomorrow",
            "--imgsize=900,1200", "--projection=ortho", "--camera=260,4,30,0,4,30",
        ],
    ),
    (
        ROOT / "preview" / "knight.png",
        [
            "-D", 'part="knight"', "--preview", "--colorscheme=Tomorrow",
            "--projection=perspective", "--imgsize=1000,1200",
            "--camera=180,160,90,0,4,28",
        ],
    ),
]

for out, extra in jobs:
    result = subprocess.run([OSC, "-o", str(out), *extra, str(SRC)], capture_output=True, text=True)
    text = (result.stderr or "") + (result.stdout or "")
    bad = [ln for ln in text.splitlines() if "ERROR" in ln or "empty" in ln or "WARNING" in ln]
    print(out.name, result.returncode, out.stat().st_size, "; ".join(bad[:4]))
