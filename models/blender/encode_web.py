"""Encode film masters into web HLS (fMP4, 2 s chunks) + fallback MP4 + posters.

Industry pattern (Apple/Stripe marketing, hls.js VOD):
  poster = LCP, HLS ABR starts after paint, below-fold streams attach near viewport.

  py -3.12 encode_web.py
  py -3.12 encode_web.py --only hero
"""
from __future__ import annotations

import json
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent
VIDEO = ROOT / "video"
ASSETS = ROOT.parents[1] / "gh-pages-ready" / "landing" / "assets"
HLS_ROOT = ASSETS / "hls"

try:
    import imageio_ffmpeg

    FFMPEG = imageio_ffmpeg.get_ffmpeg_exe()
except Exception:
    FFMPEG = shutil.which("ffmpeg") or "ffmpeg"

# 720p for first paint, 1080p for ABR upswitch. 4K stays a local master.
LADDER = (
    ("720", 1280, 720, 20, 1_800_000),
    ("1080", 1920, 1080, 19, 3_600_000),
)

FILMS = (
    {
        "name": "hero",
        "sources": (VIDEO / "predstaveni_4k.mp4", VIDEO / "predstaveni.mp4", ASSETS / "czm-hero-loop.mp4"),
        "fallback": ASSETS / "czm-hero-loop.mp4",
        "poster": ASSETS / "hero-reel-poster.webp",
        # Mid glass fly (source seconds). Skip the under-lip open.
        "poster_at": 5.5,
        # Start on glass+2 (frame 122): fly along the sandblast. End before CzechMate logo.
        "start": 4.07,
        "end": 91.00,
    },
    {
        "name": "players",
        "sources": (VIDEO / "hraci_4k.mp4", VIDEO / "hraci_720.mp4", ASSETS / "czm-players.mp4"),
        "fallback": ASSETS / "czm-players.mp4",
        "poster": ASSETS / "film-players-poster.webp",
        "poster_at": 2.0,
    },
    {
        "name": "lamp",
        "sources": (VIDEO / "lampa.mp4", ASSETS / "czm-lamp.mp4"),
        "fallback": ASSETS / "czm-lamp.mp4",
        "poster": ASSETS / "film-lamp-poster.webp",
        "poster_at": 3.5,
    },
    {
        "name": "board",
        "sources": (VIDEO / "predstaveni_4k.mp4", VIDEO / "predstaveni.mp4", ASSETS / "czm-board-film.mp4"),
        "fallback": ASSETS / "czm-board-film.mp4",
        "poster": ASSETS / "film-board-poster.webp",
        "poster_at": 18.0,
    },
    {
        "name": "build",
        "sources": (VIDEO / "rozklad_4k.mp4", VIDEO / "rozklad.mp4", ASSETS / "czm-board-build.mp4"),
        "fallback": ASSETS / "czm-board-build.mp4",
        "poster": ASSETS / "film-build-poster.webp",
        "poster_at": 11.0,
    },
    {
        "name": "led",
        "sources": (ASSETS / "czm-v2-led-loop.mp4",),
        "fallback": ASSETS / "czm-v2-led-loop.mp4",
        "poster": None,
        "poster_at": 0.4,
    },
    {
        "name": "app",
        "sources": (ASSETS / "czm-v2-app-iphone.mp4",),
        "fallback": ASSETS / "czm-v2-app-iphone.mp4",
        "poster": None,
        "poster_at": 0.8,
    },
)


def run(cmd: list[str], cwd: Path | None = None) -> None:
    print("+", " ".join(cmd[:8]), "...")
    subprocess.check_call(cmd, cwd=str(cwd) if cwd else None)


def pick_source(candidates) -> Path | None:
    for path in candidates:
        if path.is_file() and path.stat().st_size > 10_000:
            return path
    return None


def encode_rung(
    src: Path,
    dest_dir: Path,
    tag: str,
    width: int,
    height: int,
    crf: int,
    start: float | None = None,
    end: float | None = None,
) -> int:
    dest_dir.mkdir(parents=True, exist_ok=True)
    vf = (
        f"scale=w={width}:h={height}:force_original_aspect_ratio=decrease:"
        "force_divisible_by=2,setsar=1"
    )
    cmd = [FFMPEG, "-y"]
    if start is not None:
        cmd.extend(["-ss", f"{start:.3f}"])
    cmd.extend(["-i", str(src), "-an"])
    if end is not None:
        duration = end - (start or 0.0)
        cmd.extend(["-t", f"{duration:.3f}"])
    cmd.extend(
        [
            "-vf",
            vf,
            "-c:v",
            "libx264",
            "-preset",
            "slow",
            "-crf",
            str(crf),
            "-profile:v",
            "high",
            "-pix_fmt",
            "yuv420p",
            "-g",
            "60",
            "-keyint_min",
            "60",
            "-sc_threshold",
            "0",
            "-hls_time",
            "2",
            "-hls_playlist_type",
            "vod",
            "-hls_flags",
            "independent_segments",
            "-hls_segment_type",
            "fmp4",
            "-hls_fmp4_init_filename",
            "init.mp4",
            "-hls_segment_filename",
            "seg_%03d.m4s",
            "-movflags",
            "+faststart",
            "index.m3u8",
        ]
    )
    run(cmd, cwd=dest_dir)
    size = sum(p.stat().st_size for p in dest_dir.iterdir() if p.is_file())
    print(tag, dest_dir, "bytes", size)
    return size


def write_master(folder: Path, rungs: list[tuple[str, int, int, int]]) -> None:
    lines = ["#EXTM3U", "#EXT-X-VERSION:7"]
    for tag, width, height, _crf, bandwidth in rungs:
        lines.append(f"#EXT-X-STREAM-INF:BANDWIDTH={bandwidth},RESOLUTION={width}x{height},CODECS=\"avc1.640028\"")
        lines.append(f"{tag}/index.m3u8")
    (folder / "master.m3u8").write_text("\n".join(lines) + "\n", encoding="utf-8")


def encode_fallback(
    src: Path,
    dest: Path,
    width: int = 1280,
    start: float | None = None,
    end: float | None = None,
) -> None:
    tmp = dest.with_suffix(".tmp.mp4")
    cmd = [FFMPEG, "-y"]
    if start is not None:
        cmd.extend(["-ss", f"{start:.3f}"])
    cmd.extend(["-i", str(src), "-an"])
    if end is not None:
        duration = end - (start or 0.0)
        cmd.extend(["-t", f"{duration:.3f}"])
    cmd.extend(
        [
            "-vf",
            f"scale=w={width}:h=-2:force_original_aspect_ratio=decrease:force_divisible_by=2",
            "-c:v",
            "libx264",
            "-preset",
            "medium",
            "-crf",
            "21",
            "-profile:v",
            "high",
            "-pix_fmt",
            "yuv420p",
            "-bf",
            "0",
            "-movflags",
            "+faststart",
            str(tmp),
        ]
    )
    run(cmd)
    tmp.replace(dest)


def poster(src: Path, dest: Path, at: float) -> None:
    png = dest.with_suffix(".png")
    run(
        [
            FFMPEG,
            "-y",
            "-ss",
            str(at),
            "-i",
            str(src),
            "-update",
            "1",
            "-frames:v",
            "1",
            str(png),
        ]
    )
    from PIL import Image

    Image.open(png).convert("RGB").save(dest, "WEBP", quality=86, method=6)
    png.unlink(missing_ok=True)


def encode_film(spec: dict) -> dict | None:
    src = pick_source(spec["sources"])
    if src is None:
        print("skip", spec["name"], "no source")
        return None
    name = spec["name"]
    out = HLS_ROOT / name
    if out.exists():
        shutil.rmtree(out)
    out.mkdir(parents=True, exist_ok=True)
    print("encode", name, "from", src, "start", spec.get("start"), "end", spec.get("end"))
    start = spec.get("start")
    end = spec.get("end")
    rungs = []
    for tag, width, height, crf, bandwidth in LADDER:
        encode_rung(src, out / tag, tag, width, height, crf, start=start, end=end)
        rungs.append((tag, width, height, crf, bandwidth))
    write_master(out, rungs)
    fallback = spec["fallback"]
    encode_fallback(src, fallback, 1280, start=start, end=end)
    if spec["poster"] is not None:
        poster(src, spec["poster"], spec["poster_at"])
    return {"name": name, "source": str(src), "hls": str(out / "master.m3u8")}


def main() -> None:
    only = None
    if "--only" in sys.argv:
        only = sys.argv[sys.argv.index("--only") + 1]
    HLS_ROOT.mkdir(parents=True, exist_ok=True)
    done = []
    for spec in FILMS:
        if only and spec["name"] != only:
            continue
        info = encode_film(spec)
        if info:
            done.append(info)
    manifest = HLS_ROOT / "manifest.json"
    if only and manifest.is_file():
        prev = json.loads(manifest.read_text(encoding="utf-8"))
        by_name = {item["name"]: item for item in prev}
        for item in done:
            by_name[item["name"]] = item
        done = list(by_name.values())
    manifest.write_text(json.dumps(done, indent=2), encoding="utf-8")
    print("wrote", manifest, "films", len(done))


if __name__ == "__main__":
    main()
