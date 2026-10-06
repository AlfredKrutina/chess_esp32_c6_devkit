"""Transcode the 4K hraci master to web sizes. Needs imageio-ffmpeg.

  py -3.12 export_hraci_web.py
"""

import shutil
import subprocess
from pathlib import Path

import imageio_ffmpeg

ROOT = Path(__file__).resolve().parent
VIDEO = ROOT / "video"
SRC = VIDEO / "hraci_4k.mp4"
WEB = ROOT.parent.parent / "gh-pages-ready" / "landing" / "assets"
FFMPEG = imageio_ffmpeg.get_ffmpeg_exe()


def run(cmd):
    print(" ".join(str(c) for c in cmd), flush=True)
    subprocess.check_call(cmd)


def encode(src, dst, width, height, extra):
    dst.parent.mkdir(parents=True, exist_ok=True)
    tmp = dst.with_suffix(".tmp" + dst.suffix)
    cmd = [
        FFMPEG, "-y", "-i", str(src),
        "-vf", f"scale={width}:{height}:flags=lanczos",
        *extra,
        "-an",
        str(tmp),
    ]
    run(cmd)
    tmp.replace(dst)
    print("wrote", dst, dst.stat().st_size, flush=True)


def main():
    if not SRC.is_file():
        raise SystemExit(f"missing {SRC}")
    h264 = [
        "-c:v", "libx264", "-pix_fmt", "yuv420p", "-profile:v", "high",
        "-crf", "18", "-preset", "slow", "-movflags", "+faststart",
    ]
    encode(SRC, VIDEO / "hraci_web_2160.mp4", 3840, 2160, h264)
    encode(SRC, VIDEO / "hraci_web_1080.mp4", 1920, 1080, h264)
    encode(SRC, VIDEO / "hraci_web_720.mp4", 1280, 720, h264)
    encode(
        SRC,
        VIDEO / "hraci_web_1080.webm",
        1920,
        1080,
        ["-c:v", "libvpx-vp9", "-b:v", "0", "-crf", "32", "-row-mt", "1", "-pix_fmt", "yuv420p"],
    )
    site = WEB / "czm-players.mp4"
    shutil.copyfile(VIDEO / "hraci_web_720.mp4", site)
    print("site", site, site.stat().st_size, flush=True)
    poster = WEB / "film-players-poster.webp"
    run([
        FFMPEG, "-y", "-ss", "5.7", "-i", str(VIDEO / "hraci_web_1080.mp4"),
        "-frames:v", "1", "-vf", "scale=1280:720:flags=lanczos",
        "-c:v", "libwebp", "-lossless", "0", "-compression_level", "6", "-q:v", "86",
        str(poster),
    ])
    print("poster", poster, poster.stat().st_size, flush=True)


if __name__ == "__main__":
    main()
