"""Build black-PLA piece webps that share the white still's card background.

Only the sculpture pixels are remapped to dark PLA. No warm/orange tint —
neutral charcoal with soft facet light so the form stays readable.
The void/floor stay byte-identical to the white product still.
"""
from pathlib import Path

import numpy as np
from PIL import Image

SRC = Path(__file__).resolve().parents[2] / "gh-pages-ready" / "landing" / "assets"
KINDS = ("king", "queen", "bishop", "knight", "rook", "pawn")


def to_black_piece_only(im: Image.Image) -> Image.Image:
    rgba = np.asarray(im.convert("RGBA"), dtype=np.float32)
    rgb = rgba[..., :3]
    alpha = rgba[..., 3]
    lum = 0.2126 * rgb[..., 0] + 0.7152 * rgb[..., 1] + 0.0722 * rgb[..., 2]

    # Soft mask of the lit sculpture (background void stays ~0–8).
    body = np.clip((lum - 14.0) / 40.0, 0.0, 1.0)
    body = body * body * (3.0 - 2.0 * body)  # smoothstep

    shade = np.clip((lum - 14.0) / (230.0 - 14.0), 0.0, 1.0)

    # Near-black PLA: facets stay readable, body stays charcoal not pewter.
    black_l = 2.0 + (shade**2.4) * 12.0 + (shade**9.0) * 14.0
    black = np.empty_like(rgb)
    black[..., 0] = black_l
    black[..., 1] = black_l
    black[..., 2] = black_l

    # Soft cool rim from edges — definition only, not a tint.
    gy, gx = np.gradient(lum)
    edge = np.hypot(gx, gy)
    rim = np.clip((edge - 4.0) / 40.0, 0.0, 1.0)
    rim = rim * rim * (3.0 - 2.0 * rim) * body * (0.35 + 0.65 * (1.0 - shade))
    lift = rim * 8.0 + (shade**6.0) * body * 6.0
    black[..., 0] = np.clip(black[..., 0] + lift, 0.0, 255.0)
    black[..., 1] = np.clip(black[..., 1] + lift, 0.0, 255.0)
    black[..., 2] = np.clip(black[..., 2] + lift * 1.02, 0.0, 255.0)

    out = rgb.copy()
    for c in range(3):
        out[..., c] = rgb[..., c] * (1.0 - body) + black[..., c] * body

    hard = body < 0.02
    out[hard] = rgb[hard]

    packed = np.dstack([np.clip(out, 0, 255).astype(np.uint8), alpha.astype(np.uint8)])
    return Image.fromarray(packed, "RGBA").convert("RGB")


def main() -> None:
    for kind in KINDS:
        src = SRC / f"piece-{kind}.webp"
        dest = SRC / f"piece-{kind}-black.webp"
        to_black_piece_only(Image.open(src)).save(dest, "WEBP", quality=88, method=6)
        print(kind, dest.name, dest.stat().st_size)


if __name__ == "__main__":
    main()
