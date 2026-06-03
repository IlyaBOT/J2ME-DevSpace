#!/usr/bin/env python3
import argparse
import shutil
import subprocess
from pathlib import Path

PROFILES = {
    "music-mp3": ["-ac", "1", "-ar", "22050", "-b:a", "64k"],
    "music-low-mp3": ["-ac", "1", "-ar", "16000", "-b:a", "40k"],
    "sfx-wav": ["-ac", "1", "-ar", "11025", "-acodec", "pcm_u8"],
    "retro-wav": ["-ac", "1", "-ar", "8000", "-acodec", "pcm_u8"],
    "amr": ["-ac", "1", "-ar", "8000", "-ab", "12.2k"],
}
EXT = {
    "music-mp3": ".mp3",
    "music-low-mp3": ".mp3",
    "sfx-wav": ".wav",
    "retro-wav": ".wav",
    "amr": ".amr",
}

def main():
    ap = argparse.ArgumentParser(description="Convert audio to J2ME-friendly formats using FFmpeg.")
    ap.add_argument("input")
    ap.add_argument("-o", "--out", help="output file or directory")
    ap.add_argument("--profile", choices=PROFILES.keys(), default="music-mp3")
    ap.add_argument("--overwrite", action="store_true")
    ap.add_argument("--ffmpeg", default="ffmpeg")
    args = ap.parse_args()

    if shutil.which(args.ffmpeg) is None:
        raise SystemExit("ffmpeg not found in PATH")

    inp = Path(args.input)
    if args.out:
        out = Path(args.out)
        if out.is_dir() or str(args.out).endswith(('/', '\\')):
            out.mkdir(parents=True, exist_ok=True)
            out = out / (inp.stem + EXT[args.profile])
    else:
        out = inp.with_suffix(EXT[args.profile])

    cmd = [args.ffmpeg]
    if args.overwrite:
        cmd.append("-y")
    cmd += ["-i", str(inp)] + PROFILES[args.profile] + [str(out)]
    print(" ".join(cmd))
    subprocess.check_call(cmd)
    print(out)

if __name__ == "__main__":
    main()
