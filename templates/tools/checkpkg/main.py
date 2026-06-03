#!/usr/bin/env python3
import argparse
import zipfile
from pathlib import Path

def parse_jad(path):
    d = {}
    for line in Path(path).read_text(errors="replace").splitlines():
        if ':' in line:
            k, v = line.split(':', 1)
            d[k.strip()] = v.strip()
    return d

def main():
    ap = argparse.ArgumentParser(description="Check J2ME package basics.")
    ap.add_argument("path", help="JAR or JAD")
    ap.add_argument("--max-kb", type=int, default=1024)
    args = ap.parse_args()
    p = Path(args.path)
    jar = p if p.suffix.lower() == ".jar" else None
    jad = p if p.suffix.lower() == ".jad" else None
    if jad:
        meta = parse_jad(jad)
        print("JAD:", jad)
        for k in ["MIDlet-Name", "MIDlet-Version", "MIDlet-Vendor", "MicroEdition-Profile", "MicroEdition-Configuration", "MIDlet-Jar-URL", "MIDlet-Jar-Size"]:
            print(f"  {k}: {meta.get(k, 'MISSING')}")
        if "MIDlet-Jar-URL" in meta:
            jar = jad.parent / meta["MIDlet-Jar-URL"]
    if jar and jar.exists():
        size_kb = jar.stat().st_size / 1024
        print(f"JAR: {jar} ({size_kb:.1f} KiB)")
        if size_kb > args.max_kb:
            print(f"WARNING: JAR is larger than {args.max_kb} KiB")
        with zipfile.ZipFile(jar) as z:
            names = z.namelist()
            print(f"Entries: {len(names)}")
            print("Manifest:", "META-INF/MANIFEST.MF" in names)
    else:
        raise SystemExit("JAR not found")

if __name__ == "__main__":
    main()
