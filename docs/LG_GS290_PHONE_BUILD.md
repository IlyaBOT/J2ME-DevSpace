# LG GS290: CLDC-compatible builds

This improves the **PC build environment**, not the AROMA Java runtime in the
phone's firmware. Earlier builds used Java 8 class files (version 52.0) and
sometimes skipped CLDC preverification, which can cause an installed MIDlet to
show "Not supported".

## Corrected default DevSpace demo

The existing `j2me_devspace-demo` is the only example project installed
by workspace setup/update. Its original LCDUI form, text inputs, gauges,
choices, scrolling and commands remain intact.

- Compile with ECJ `-source 1.3 -target cldc1.1` for class-file **45.3**
  and the CLDC `StackMap` verifier attribute.
- Link against CLDC 1.1 and MIDP 2.0 API references. Java ME API stubs,
  ECJ, and a host JDK are build-time tools, not files to place in the JAR.
- The profile declares CLDC 1.1 / MIDP 2.0 as a compatible minimum; V10C
  firmware appears to provide a newer MIDP implementation.
- Correct missing icon reference to `/icons/icon.png`; no HTTP permission.
- Validate ZIP, manifest, referenced MIDlet class/icon, all class versions,
  JAD jar filename and jar size.
- For GS290 touch-enabled profiles, include stock LG JAD/manifest fields:
  `MIDlet-Touch-Support: true`, `LGE-MIDlet-Width: 240`, and
  `LGE-MIDlet-Height: 400`. These were found in V10C factory JADs.
  This does not add an independent touch demonstration to the workspace.

## Windows setup

Rebuild the C++ CLI after merging, then from an existing workspace:

```powershell
j2me --workspace update "I:\J2ME-Workspace"
Set-Location "I:\J2ME-Workspace"
j2me --build j2me_devspace-demo --profile lg-gs290
```

The build requires Python 3 and a Java launcher (host JDK 8+) and downloads
ECJ 3.26.0, CLDC API references and MIDP API references from Maven Central
into `sdk/phonebuild/`. The compiler and MIDP API dependencies have pinned
SHA-256 checks. No APIs are bundled into the installed JAR.

Files:
```text
projects/j2me_devspace-demo/dist/j2me_devspace-demo.jar
projects/j2me_devspace-demo/dist/j2me_devspace-demo.jad
```

To inspect the resulting package:

```powershell
python .\tools\phonebuild\main.py --inspect-jar .\projects\j2me_devspace-demo\dist\j2me_devspace-demo.jar
```

If the phone rejects a new MIDlet, compare its JAD and JAR against
known-working packages from the same handset. Do not change firmware for
a toolchain problem.

This patch does not include or merge the independent GS290 touch-test
application; that app remains experimental.
