"""Derive dark-PLA piece webps from the white product stills (same crop/lighting)."""
from pathlib import Path

import numpy as np
from PIL import Image

SRC = Path(__file__).resolve().parents[2] / "gh-pages-ready" / "landing" / "assets"
KINDS = ("king", "queen", "bishop", "knight", "rook", "pawn")


def to_black(im: Image.Image) -> Image.Image:
    arr = np.asarray(im.convert("RGBA"), dtype=np.float32)
    rgb = arr[..., :3]
    alpha = arr[..., 3]
    lum = 0.2126 * rgb[..., 0] + 0.7152 * rgb[..., 1] + 0.0722 * rgb[..., 2]
    # Soft body mask: keep void + soft floor, remap the lit sculpture.
    body = np.clip((lum - 8.0) / 28.0, 0.0, 1.0)
    shade = np.clip(lum / 225.0, 0.0, 1.0)
    # Near-black PLA: deep body, only facet ridges pick up a cool highlight.
    black_l = 4.0 + (shade**1.85) * 28.0 + (shade**6.0) * 70.0
    out = np.empty_like(rgb)
    out[..., 0] = black_l * 0.92
    out[..., 1] = black_l * 0.95
    out[..., 2] = black_l * 1.02
    for c in range(3):
        out[..., c] = rgb[..., c] * (1.0 - body) + out[..., c] * body
    rgba = np.dstack([np.clip(out, 0, 255).astype(np.uint8), alpha.astype(np.uint8)])
    return Image.fromarray(rgba, "RGBA").convert("RGB")


def main() -> None:
    for kind in KINDS:
        src = SRC / f"piece-{kind}.webp"
        dest = SRC / f"piece-{kind}-black.webp"
        to_black(Image.open(src)).save(dest, "WEBP", quality=86, method=6)
        print(kind, dest.name, dest.stat().st_size)


if __name__ == "__main__":
    main()
