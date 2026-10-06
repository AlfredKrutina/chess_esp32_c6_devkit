"""Copy 4K product stills into landing webps (keep masters as JPG in video/web_stills)."""
from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parent
SRC = ROOT / "video" / "web_stills"
DEST = ROOT.parents[1] / "gh-pages-ready" / "landing" / "assets"
KINDS = ("rook", "knight", "bishop", "queen", "king", "pawn")


def webp(src: Path, dest: Path, quality: int = 88) -> None:
    im = Image.open(src).convert("RGB")
    dest.parent.mkdir(parents=True, exist_ok=True)
    im.save(dest, "WEBP", quality=quality, method=6)
    print(dest.name, dest.stat().st_size)


def main() -> None:
    if not SRC.is_dir():
        raise SystemExit(f"missing {SRC}")
    lineup = SRC / "lineup.jpg"
    if lineup.is_file():
        webp(lineup, DEST / "og-share.webp", 86)
        # PNG OG remains for crawlers that skip webp.
        Image.open(lineup).convert("RGB").save(DEST / "og-share.png", "PNG", optimize=True)
        print("og-share.png", (DEST / "og-share.png").stat().st_size)
    for kind in KINDS:
        src = SRC / f"{kind}.jpg"
        if src.is_file():
            webp(src, DEST / f"piece-{kind}.webp")


if __name__ == "__main__":
    main()
