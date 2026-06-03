#!/usr/bin/env python3
import argparse
from pathlib import Path
from PIL import Image

DITHER = {
    "none": Image.Dither.NONE,
    "floyd": Image.Dither.FLOYDSTEINBERG,
}

def nearest_palette(img, colors, dither):
    return img.convert("RGB").quantize(colors=colors, method=Image.Quantize.MEDIANCUT, dither=dither)

def gray_removed(img, colors, dither):
    # Practical mode: reduce to grayscale palette. It is predictable on weak phones.
    return img.convert("L").quantize(colors=min(colors, 256), dither=dither)

def websafe(img, dither):
    return img.convert("RGB").quantize(palette=Image.new("P", (1, 1)), dither=dither)

def main():
    ap = argparse.ArgumentParser(description="Reduce image colors for J2ME assets.")
    ap.add_argument("input")
    ap.add_argument("-o", "--out")
    ap.add_argument("--colors", type=int, default=256)
    ap.add_argument("--method", choices=["nearest", "gray", "median-cut", "websafe"], default="nearest")
    ap.add_argument("--dither", choices=DITHER.keys(), default="none")
    ap.add_argument("--resize", help="optional WxH resize before quantization")
    args = ap.parse_args()

    img = Image.open(args.input).convert("RGBA")
    if args.resize:
        w, h = map(int, args.resize.lower().split("x"))
        img = img.resize((w, h), Image.Resampling.LANCZOS)

    d = DITHER[args.dither]
    if args.method in ("nearest", "median-cut"):
        out = nearest_palette(img, args.colors, d)
    elif args.method == "gray":
        out = gray_removed(img, args.colors, d)
    else:
        out = websafe(img, d)

    out_path = Path(args.out) if args.out else Path(args.input).with_name(Path(args.input).stem + f".{args.method}.{args.colors}.png")
    out_path.parent.mkdir(parents=True, exist_ok=True)
    out.save(out_path, optimize=True)
    print(out_path)

if __name__ == "__main__":
    main()
