#!/usr/bin/env python3
import argparse
from pathlib import Path
from PIL import Image, ImageOps

RESAMPLE = {
    "nearest": Image.Resampling.NEAREST,
    "bilinear": Image.Resampling.BILINEAR,
    "bicubic": Image.Resampling.BICUBIC,
    "lanczos": Image.Resampling.LANCZOS,
}

def parse_sizes(s: str):
    out = []
    for item in s.split(','):
        item = item.strip().lower()
        if not item:
            continue
        if 'x' in item:
            w, h = item.split('x', 1)
            out.append((int(w), int(h)))
        else:
            n = int(item)
            out.append((n, n))
    return out

def main():
    ap = argparse.ArgumentParser(description="Generate J2ME icons from PNG/JPG.")
    ap.add_argument("input", help="source image")
    ap.add_argument("-o", "--out", default="res/icons", help="output directory")
    ap.add_argument("--sizes", default="16,24,32,48,64", help="comma-separated sizes, e.g. 16,32,48 or 32x32")
    ap.add_argument("--method", choices=RESAMPLE.keys(), default="lanczos")
    ap.add_argument("--pad", action="store_true", help="pad to square instead of center-crop")
    ap.add_argument("--palette", type=int, default=0, help="optional PNG palette colors, e.g. 256")
    args = ap.parse_args()

    src = Image.open(args.input).convert("RGBA")
    out_dir = Path(args.out)
    out_dir.mkdir(parents=True, exist_ok=True)

    for w, h in parse_sizes(args.sizes):
        if args.pad:
            img = ImageOps.contain(src, (w, h), RESAMPLE[args.method])
            canvas = Image.new("RGBA", (w, h), (0, 0, 0, 0))
            canvas.alpha_composite(img, ((w - img.width) // 2, (h - img.height) // 2))
            img = canvas
        else:
            img = ImageOps.fit(src, (w, h), RESAMPLE[args.method])
        if args.palette:
            img = img.convert("P", palette=Image.Palette.ADAPTIVE, colors=args.palette)
        path = out_dir / f"icon{w}x{h}.png"
        img.save(path, optimize=True)
        if w == h:
            alias = out_dir / f"icon{w}.png"
            img.save(alias, optimize=True)
        print(path)

if __name__ == "__main__":
    main()
