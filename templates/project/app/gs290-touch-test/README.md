# GS290 Touch Test

Unprivileged CLDC 1.1 / MIDP 2.0 diagnostic MIDlet for the 240×400 LG GS290.

Shows two-line hello text, current Canvas dimensions, touch availability, pointer coordinates,
drag events, key events, and touch count. No network, file, vendor APIs, or native code.

Build using `j2me --build gs290-touch-test --profile lg-gs290` after updating
the workspace and installing the new CLI from this branch. Or run the standalone
`python tools/phonebuild/main.py --workspace <workspace> --project <project>`.

If the handset shows `Unsupported` before rendering, inspect the generated JAR
with `python tools/phonebuild/main.py --inspect-jar <path-to-JAR>`.
The resulting JAR must contain Java 45.3 bytecode and a valid MIDlet-1 class.
