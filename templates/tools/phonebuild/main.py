#!/usr/bin/env python3
"""Produce phone-ready CLDC 1.1 / MIDP 2.0 JAR+JAD using Eclipse ECJ.

No Java 8 class files, no silent unpreverified fallback. Requires host Python 3
and a Java 8+ launcher. Downloads unmodified Maven Central build-time tools
into the workspace; none are packaged inside the handset JAR.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import struct
import subprocess
import sys
import urllib.request
import zipfile

ECJ_VERSION = "3.26.0"
MAVEN = ("https://repo.maven.apache.org/maven2/", "https://repo1.maven.org/maven2/")
DEPENDENCIES = {
    "ecj.jar": (
        "org/eclipse/jdt/ecj/3.26.0/ecj-3.26.0.jar",
        "ac0ba5876eaf7ebb47749a0d1be179c51f194b9dd0b875d1c09e1b530f5a2db5"),
    "cldcapi11.jar": ("org/microemu/cldcapi11/2.0.4/cldcapi11-2.0.4.jar", None),
    "midpapi20.jar": (
        "org/microemu/midpapi20/2.0.4/midpapi20-2.0.4.jar",
        "b7f6ed5e0344c7000643ecc114e6edff3587e8e77face546af0e12ffd6dbe826"),
}

def die(message):
    raise SystemExit("[PHONEBUILD ERROR] " + message)

def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def acquire(name, cache):
    rel, expected = DEPENDENCIES[name]
    dest = cache / name
    if dest.exists():
        if expected and digest(dest) != expected:
            die("Checksum mismatch for cached " + str(dest))
        try:
            with zipfile.ZipFile(dest) as z:
                if z.testzip() is None:
                    return dest
        except zipfile.BadZipFile:
            pass
        die("Invalid JAR in cache: " + str(dest))
    cache.mkdir(parents=True, exist_ok=True)
    errors = []
    for host in MAVEN:
        tmp = cache / (name + ".part")
        try:
            with urllib.request.urlopen(host + rel, timeout=90) as resp:
                with tmp.open("wb") as fh:
                    shutil.copyfileobj(resp, fh)
            if expected and digest(tmp) != expected:
                die("SHA256 mismatch for downloaded " + name)
            with zipfile.ZipFile(tmp) as z:
                invalid = z.testzip()
                if invalid:
                    die("Corrupt entry in " + name + ": " + invalid)
            tmp.replace(dest)
            print("[SDK]", name, "downloaded from Maven Central")
            return dest
        except (OSError, zipfile.BadZipFile, ValueError) as exc:
            errors.append(str(exc))
            tmp.unlink(missing_ok=True)
    die("Cannot obtain " + name + ": " + "; ".join(errors))

def txt(value, label):
    s = str(value)
    if "\r" in s or "\n" in s or "\x00" in s:
        die("Newlines or NUL in descriptor field: " + label)
    # Avoid invalid old-phone descriptor encodings and line wrapping.
    try:
        raw = s.encode("ascii")
    except UnicodeEncodeError:
        die("Use ASCII for MIDlet metadata on first compatibility build: " + label)
    if len(raw) > 120:
        die("Descriptor field too long: " + label)
    return s

def descriptor(data):
    lines = []
    for key, value in data:
        key = txt(key, key)
        value = txt(value, key)
        line = key + ": " + value
        if len(line.encode("ascii")) > 70:
            die("Descriptor line exceeds conservative 70-byte limit: " + key)
        lines.append(line)
    return ("\r\n".join(lines) + "\r\n").encode("ascii")

def class_versions(jar):
    classes = []
    with zipfile.ZipFile(jar) as z:
        for item in z.infolist():
            if not item.filename.endswith(".class"):
                continue
            data = z.read(item.filename)
            if len(data) < 10 or data[:4] != b"\xCA\xFE\xBA\xBE":
                die("Invalid class file: " + item.filename)
            minor, major = struct.unpack(">HH", data[4:8])
            classes.append((item.filename, major, minor, b"StackMap" in data))
    return classes

def inspect(jar):
    with zipfile.ZipFile(jar) as z:
        manifest = z.read("META-INF/MANIFEST.MF").decode("utf-8", "replace")
        lines = dict(line.split(": ", 1) for line in manifest.splitlines()
                     if ": " in line and not line.startswith(" "))
        print("[JAR]", jar, "size", jar.stat().st_size, "bytes")
        print("[META]", "MIDlet-1 =", lines.get("MIDlet-1"),
              "CLDC =", lines.get("MicroEdition-Configuration"),
              "MIDP =", lines.get("MicroEdition-Profile"))
        midlet = lines.get("MIDlet-1", "").split(",")
        if len(midlet) != 3:
            die("MIDlet-1 must contain name, icon and class")
        klass = midlet[2].strip().replace(".", "/") + ".class"
        if klass not in z.namelist():
            die("MIDlet class missing from JAR: " + klass)
        icon = midlet[1].strip().lstrip("/")
        if icon and icon not in z.namelist():
            die("Icon missing from JAR: " + icon)
    classes = class_versions(jar)
    if not classes:
        die("JAR has no Java class files")
    versions = sorted(set((major, minor) for _, major, minor, _ in classes))
    print("[BYTECODE]", versions, "classes:", len(classes),
          "with StackMap markers:", sum(x[3] for x in classes))
    if versions != [(45, 3)]:
        die("Not a phone-compatible CLDC 45.3 build; do NOT install it")
    return classes

def find_java(workspace, specified):
    if specified:
        java = Path(specified)
        if not java.is_file():
            die("Java executable not found: " + str(java))
        return str(java)
    names = ["java.exe", "java"] if os.name == "nt" else ["java"]
    for home in [workspace / "sdk" / "jdk" / "current",
                 Path(os.environ.get("JAVA_HOME", "/nonexistent"))]:
        for name in names:
            path = home / "bin" / name
            if path.is_file():
                return str(path)
    found = shutil.which("java")
    if not found:
        die("No Java launcher. Install JDK 8+ or pass --java PATH")
    return found

def build(project, workspace, java_path, cache, profile_path):
    proj = json.loads((project / "j2me.proj.json").read_text(encoding="utf-8"))
    profile = json.loads(profile_path.read_text(encoding="utf-8"))
    if proj.get("schema") != "j2me.proj/1" or profile.get("schema") != "j2me.profile/1":
        die("Invalid metadata schema")
    if profile.get("build_backend") != "ecj-cldc":
        die("Profile does not opt in to verified CLDC build_backend=ecj-cldc")
    if profile.get("cldc") != "1.1" or profile.get("midp") != "2.0":
        die("Initial phone build supports declared CLDC-1.1 / MIDP-2.0 only")
    sources = sorted((project / "src").rglob("*.java"))
    if not sources:
        die("No Java source files found")
    java = find_java(workspace, java_path)
    sdk = {key: acquire(key, cache) for key in DEPENDENCIES}
    out = project / "build" / "phone-cldc-classes"
    if out.exists():
        shutil.rmtree(out)
    out.mkdir(parents=True)
    cmd = [java, "-jar", str(sdk["ecj.jar"]),
           "-source", "1.3", "-target", "cldc1.1", "-g:none", "-encoding", "UTF-8",
           "-bootclasspath", os.pathsep.join((str(sdk["cldcapi11.jar"]),
                                             str(sdk["midpapi20.jar"]))),
           "-d", str(out)] + [str(p) for p in sources]
    print("[ECJ] compiling", len(sources), "files for CLDC-1.1 / MIDP-2.0")
    rc = subprocess.call(cmd)
    if rc:
        die("ECJ failed (exit code " + str(rc) + ")")
    class_files = sorted(out.rglob("*.class"))
    if not class_files:
        die("ECJ produced no class files")
    for klass in class_files:
        data = klass.read_bytes()
        if len(data) < 8 or data[:4] != b"\xCA\xFE\xBA\xBE":
            die("Invalid compiler output: " + str(klass))
        minor, major = struct.unpack(">HH", data[4:8])
        if (major, minor) != (45, 3):
            die("Unexpected class version %d.%d: %s" % (major, minor, klass))

    name = txt(proj.get("midlet_name", "MIDlet"), "midlet_name")
    klass = txt(proj["midlet_class"], "midlet_class")
    vendor = txt(proj.get("vendor", "Unknown"), "vendor")
    version = txt(proj.get("version", "0.1.0"), "version")
    jar_name = txt(proj.get("jar_name", "application.jar"), "jar_name")
    if Path(jar_name).name != jar_name or not jar_name.lower().endswith(".jar"):
        die("jar_name must be a simple .jar filename")
    icon = txt(proj.get("icon", ""), "icon").lstrip("/")
    res = project / "res"
    if icon and not (res / icon).is_file():
        die("Icon mentioned in metadata does not exist: res/" + icon)
    permissions = proj.get("permissions", [])
    if not isinstance(permissions, list):
        die("permissions must be an array")

    midlet_line = name + ", " + ("/" + icon if icon else "") + ", " + klass
    entries = [
        ("Manifest-Version", "1.0"),
        ("MIDlet-1", midlet_line),
        ("MIDlet-Name", name),
        ("MIDlet-Version", version),
        ("MIDlet-Vendor", vendor),
        ("MicroEdition-Configuration", "CLDC-1.1"),
        ("MicroEdition-Profile", "MIDP-2.0"),
    ]
    if permissions:
        entries.append(("MIDlet-Permissions", ", ".join(map(str, permissions))))
    if icon:
        entries.append(("MIDlet-Icon", "/" + icon))
    dist = project / "dist"
    dist.mkdir(exist_ok=True)
    jar = dist / jar_name
    with zipfile.ZipFile(jar, "w", compression=zipfile.ZIP_DEFLATED, compresslevel=6) as z:
        z.writestr("META-INF/MANIFEST.MF", descriptor(entries))
        for klass_file in class_files:
            z.write(klass_file, klass_file.relative_to(out).as_posix())
        if res.is_dir():
            for asset in sorted(res.rglob("*")):
                if asset.is_file() and asset.name != ".gitkeep":
                    z.write(asset, asset.relative_to(res).as_posix())
    inspect(jar)
    max_kb = int(profile.get("jar_size_limit_kb", 1024))
    if jar.stat().st_size > max_kb * 1024:
        die("JAR exceeds configured profile limit: %d KB" % max_kb)
    jad = dist / (Path(jar_name).stem + ".jad")
    jad.write_bytes(descriptor(entries + [
        ("MIDlet-Jar-URL", jar_name),
        ("MIDlet-Jar-Size", str(jar.stat().st_size)),
    ]))
    print("[PHONEBUILD OK]", jar)
    print("[JAD]", jad, "size field", jar.stat().st_size)
    return 0

def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--project", type=Path)
    p.add_argument("--workspace", type=Path)
    p.add_argument("--profile", type=Path)
    p.add_argument("--java", default="")
    p.add_argument("--inspect-jar", type=Path)
    args = p.parse_args()
    if args.inspect_jar:
        inspect(args.inspect_jar)
        return 0
    if not args.project or not args.workspace:
        die("Specify --project and --workspace (or --inspect-jar)")
    project = args.project.resolve()
    workspace = args.workspace.resolve()
    profile = args.profile or (project / "j2me.profile-lg-gs290.json")
    cache = workspace / "sdk" / "phonebuild"
    return build(project, workspace, args.java, cache, profile.resolve())

if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, KeyError, ValueError, zipfile.BadZipFile, json.JSONDecodeError) as exc:
        die(str(exc))
