#!/usr/bin/env python3
"""
Ořez figurek CzechMate: odstranění světlého pozadí, trim průhledných okrajů,
zmenšení pro web (embed) + přepsání iOS Assets.

Vyžaduje: pip install pillow

Spuštění z kořene repozitáře:
  python3 components/web_server_task/tools/process_piece_pngs.py
"""

from __future__ import annotations

import os
import shutil
import sys

try:
    from PIL import Image
except ImportError:
    print("Install Pillow: pip install pillow", file=sys.stderr)
    sys.exit(1)

# Relativně ke kořeni repozitáře (parent parent ... z tohoto souboru)
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
REPO_ROOT = os.path.normpath(os.path.join(SCRIPT_DIR, "..", "..", ".."))

PIECES = [
    "PieceWhiteKing",
    "PieceWhiteQueen",
    "PieceWhiteRook",
    "PieceWhiteBishop",
    "PieceWhiteKnight",
    "PieceWhitePawn",
    "PieceBlackKing",
    "PieceBlackQueen",
    "PieceBlackRook",
    "PieceBlackBishop",
    "PieceBlackKnight",
    "PieceBlackPawn",
]

MAX_SIDE = 192
# Pixely „jako pozadí“: světlé / šedé — zprůhlednit
BG_THRESHOLD = 38


def remove_flat_background_rgba(im: Image.Image) -> Image.Image:
    """RGB(A) -> RGBA; světlé okrajové pozadí zprůhlednit + jemný práh na tělo figurky."""
    im = im.convert("RGBA")
    px = im.load()
    w, h = im.size
    for y in range(h):
        for x in range(w):
            r, g, b, a = px[x, y]
            if a == 0:
                continue
            # odstranit téměř bílé / světle šedé (typické u AI pozadí)
            mx = max(r, g, b)
            mn = min(r, g, b)
            if mx >= 250 and (mx - mn) <= 12:
                px[x, y] = (r, g, b, 0)
                continue
            if mx > 235 and (mx - mn) < 25 and (r + g + b) > 680:
                px[x, y] = (r, g, b, 0)
                continue
            # velmi světlé krémové pozadí
            if r > 230 and g > 225 and b > 210 and (r - b) < 45:
                px[x, y] = (r, g, b, 0)
    return im


def trim_alpha(im: Image.Image, pad: int = 2) -> Image.Image:
    """Ořez podle alfa kanálu."""
    im = im.convert("RGBA")
    alpha = im.split()[3]
    bbox = alpha.getbbox()
    if bbox is None:
        return im
    l, t, r, b = bbox
    l = max(0, l - pad)
    t = max(0, t - pad)
    r = min(im.width, r + pad)
    b = min(im.height, b + pad)
    return im.crop((l, t, r, b))


def scale_down(im: Image.Image, max_side: int) -> Image.Image:
    w, h = im.size
    m = max(w, h)
    if m <= max_side:
        return im
    scale = max_side / float(m)
    nw = max(1, int(round(w * scale)))
    nh = max(1, int(round(h * scale)))
    return im.resize((nw, nh), Image.Resampling.LANCZOS)


def process_one(src_path: str, dst_path: str) -> None:
    im = Image.open(src_path)
    im = remove_flat_background_rgba(im)
    im = trim_alpha(im, pad=2)
    im = scale_down(im, MAX_SIDE)
    os.makedirs(os.path.dirname(dst_path), exist_ok=True)
    im.save(dst_path, format="PNG", optimize=True, compress_level=9)
    sz = os.path.getsize(dst_path)
    print(f"OK {os.path.basename(dst_path)} -> {im.size[0]}x{im.size[1]} {sz // 1024} KB")


def main() -> None:
    ios_base = os.path.join(
        REPO_ROOT,
        "CZECHMATE",
        "CZECHMATE",
        "Assets.xcassets",
    )
    watch_base = os.path.join(
        REPO_ROOT,
        "CZECHMATE",
        "CZECHMATEWatchApp",
        "Assets.xcassets",
    )
    out_web = os.path.join(
        REPO_ROOT,
        "components",
        "web_server_task",
        "piece_assets",
    )
    for name in PIECES:
        src = os.path.join(ios_base, f"{name}.imageset", f"{name}.png")
        if not os.path.isfile(src):
            print(f"Missing: {src}", file=sys.stderr)
            sys.exit(1)
        dst_ios = os.path.join(ios_base, f"{name}.imageset", f"{name}.png")
        dst_web = os.path.join(out_web, f"{name}.png")
        process_one(src, dst_ios)
        # stejné pixely do piece_assets (kopie po zpracování)
        im = Image.open(dst_ios)
        os.makedirs(out_web, exist_ok=True)
        im.save(dst_web, format="PNG", optimize=True, compress_level=9)
        watch_imageset = os.path.join(watch_base, f"{name}.imageset")
        if os.path.isdir(watch_imageset):
            dst_watch = os.path.join(watch_imageset, f"{name}.png")
            shutil.copy2(dst_ios, dst_watch)
            print(f"  -> Watch: {name}.png")
    print("Done. iOS + piece_assets updated.")


if __name__ == "__main__":
    main()
