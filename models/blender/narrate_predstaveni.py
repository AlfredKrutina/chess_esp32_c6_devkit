"""Timed Czech neural VO onto predstaveni.mp4. Does not overwrite the silent master."""

import asyncio
import subprocess
from pathlib import Path

import edge_tts
import imageio_ffmpeg

ROOT = Path(__file__).resolve().parent
VIDEO = ROOT / "video" / "predstaveni.mp4"
OUT = ROOT / "video" / "predstaveni_namluvene.mp4"
TMP = Path.home() / "AppData" / "Local" / "Temp" / "czechmate_render" / "vo"
FFMPEG = imageio_ffmpeg.get_ffmpeg_exe()
VOICE = "cs-CZ-AntoninNeural"

# start times; each line may run until the next start minus a breath
BEATS = [
    (0.12, "CzechMate, šachovnice která ví."),
    (3.35, "Projde všech šedesát čtyři polí, žlutá ukáže čím táhnout."),
    (8.15, "Magnetický kód v každé figurce, Hall senzory poznají kterou."),
    (13.20, "Zvedneš, zelená kam smí, položíš, modrý záblesk a šedá vlna."),
    (18.15, "Černý odpovídá stejným jazykem."),
    (21.20, "Šach, růžová na králi, žlutá na řešeních."),
    (26.25, "Teď svítí jen cé šest."),
    (30.20, "Mimo pravidla, tma, pak červená."),
    (35.25, "Modrá ukáže odkud, zpět na ef tři."),
    (38.25, "Braní, oranžová cíl, fialová oběť."),
    (41.30, "Rošáda, dva tahy, král, věž, zlaté potvrzení."),
    (49.30, "O pár tahů později, věž po e sloupci."),
    (54.30, "Mat. Vlna od vítězného krále. CzechMate."),
]
END = 59.65


def run(cmd):
    subprocess.check_call(cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)


def duration(path):
    probe = FFMPEG.replace("ffmpeg.exe", "ffprobe.exe")
    cmd = [
        FFMPEG, "-i", str(path),
        "-f", "null", "-"
    ]
    # parse from ffmpeg -i via a small python helper: use edge wav length after sox-less ffprobe fallback
    out = subprocess.check_output(
        [FFMPEG, "-i", str(path), "-f", "null", "-"],
        stderr=subprocess.STDOUT,
        text=True,
        errors="replace",
    )
    # Duration: 00:00:03.12
    for line in out.splitlines():
        if "Duration:" in line:
            hms = line.split("Duration:")[1].split(",")[0].strip()
            h, m, s = hms.split(":")
            return float(h) * 3600 + float(m) * 60 + float(s)
    raise RuntimeError(f"no duration in {path}")


async def speak(text, dest):
    communicate = edge_tts.Communicate(text, VOICE, rate="+6%", pitch="-1Hz")
    await communicate.save(str(dest))


async def main():
    TMP.mkdir(parents=True, exist_ok=True)
    clips = []
    for i, (start, text) in enumerate(BEATS):
        end = (BEATS[i + 1][0] - 0.18) if i + 1 < len(BEATS) else END
        raw = TMP / f"line_{i:02d}.mp3"
        wav = TMP / f"line_{i:02d}.wav"
        fitted = TMP / f"line_{i:02d}_fit.wav"
        await speak(text, raw)
        run([FFMPEG, "-y", "-i", str(raw), "-ac", "1", "-ar", "48000", str(wav)])
        length = duration(wav)
        budget = max(0.40, end - start)
        chain = ["highpass=f=70", "lowpass=f=11000", "acompressor=threshold=-20dB:ratio=2.2:attack=12:release=90"]
        used = length
        if length > budget * 1.02:
            tempo = min(1.10, length / budget)
            chain.append(f"atempo={tempo:.4f}")
            used = length / tempo
        if used > budget:
            used = budget
            chain.append(f"atrim=0:{budget:.3f}")
        # Fade-out must name its start. Without it this ffmpeg build fades the whole line to silence.
        fade_out = 0.06
        chain.append("afade=t=in:st=0:d=0.03")
        chain.append(f"afade=t=out:st={max(0.05, used - fade_out):.3f}:d={fade_out}")
        run([FFMPEG, "-y", "-i", str(wav), "-filter:a", ",".join(chain), str(fitted)])
        clips.append((start, fitted, duration(fitted)))
        cut = " TRIM" if length > budget * 1.12 else ""
        print(f"{i:02d} {start:5.2f}-{end:5.2f} src={length:.2f}s used={clips[-1][2]:.2f}s{cut}  {text}")

    # delay each clip and mix
    inputs = []
    filters = []
    for i, (start, path, _) in enumerate(clips):
        inputs += ["-i", str(path)]
        delay_ms = int(round(start * 1000))
        filters.append(f"[{i}]adelay={delay_ms}|{delay_ms}[a{i}]")
    mix_in = "".join(f"[a{i}]" for i in range(len(clips)))
    filters.append(f"{mix_in}amix=inputs={len(clips)}:dropout_transition=0:normalize=0,alimiter=limit=0.95[vo]")
    vo = TMP / "voiceover.wav"
    run([
        FFMPEG, "-y", *inputs,
        "-filter_complex", ";".join(filters),
        "-map", "[vo]", "-ar", "48000", "-ac", "1", str(vo),
    ])
    picture = duration(VIDEO)
    run([
        FFMPEG, "-y",
        "-i", str(VIDEO),
        "-i", str(vo),
        "-filter_complex", f"[1:a]volume=1.15,alimiter=limit=0.95,apad=whole_dur={picture:.3f}[a]",
        "-map", "0:v", "-map", "[a]",
        "-c:v", "copy",
        "-c:a", "aac", "-b:a", "192k", "-ar", "48000",
        "-t", f"{picture:.3f}",
        "-movflags", "+faststart",
        str(OUT),
    ])
    print("wrote", OUT)


if __name__ == "__main__":
    asyncio.run(main())
