# LG GS290: phone-ready J2ME + touchscreen

**This is not a replacement JRE.** LG GS290 already has AROMA Java ME VM in firmware.
The PC toolchain must generate CLDC-compatible 45.3 class files with StackMap;
Java 8 \`javac source=8 target=8\` produces 52.0 classes and will not run.

## Confirmed device target

- 240x400 WQVGA resistive touchscreen (LG published specification)
- CLDC 1.1
- MIDP 2.1 reported by GS290 firmware; request MIDP 2.0 as minimum for broad compatibility
- Untrusted MIDlet without optional JSRs, permissions, vendor APIs, or a required icon
- Touch via standard \`Canvas.pointerPressed\`, \`pointerDragged\`, \`pointerReleased\`.

## New workspace

Build/install CLI from this branch using the root README (CMake), then:

\`\`\`powershell
j2me --workspace setup 'I:\J2ME-Workspace'
Set-Location 'I:\J2ME-Workspace'
j2me --build gs290-touch-test --profile lg-gs290
\`\`\`

Python 3 (on PATH) and a Java launcher (JDK 8+) are required.
First use downloads the following *build-only* JARs from Maven Central:
ECJ 3.26.0, MicroEmulator CLDC 1.1 API stubs 2.0.4, and MIDP 2.0 API stubs 2.0.4.
These are **not** packaged into the MIDlet and are **not** run on the phone.
ECJ SHA-256 and MIDP API SHA-256 are pinned in the source code.

Generated files:

\`\`\`
projects/gs290-touch-test/dist/GS290TouchTest.jar
projects/gs290-touch-test/dist/GS290TouchTest.jad
\`\`\`

Install via the device's supported JAD/JAR file installation method. Copy
both files together; keep the JAD's \`MIDlet-Jar-URL\` pointing to the JAR.
Use the **new** JAR; do not test the previous Java 8 one.

Expected screen: black background, white text ("Hello, LG World!") plus touch
capability, screen size and pointer coordinates. No permissions requested.
Canvas \`getWidth()/getHeight()\` report available Java viewport, which may be
smaller than the panel's physical 240x400 due to softkeys.

## Existing workspace

\`\`\`powershell
# Rebuild the updated CLI and run:
j2me --workspace update 'I:\J2ME-Workspace'
Set-Location 'I:\J2ME-Workspace'
j2me --build gs290-touch-test --profile lg-gs290
\`\`\`

The update adds \`tools/phonebuild\` and \`projects/gs290-touch-test\` if missing.
It **does not overwrite existing projects or profiles**. Historical
\`lg-gs290\` profiles are intentionally routed to the new CLDC builder.
Historical \`/icons/icon32.png\` in the demo is corrected at packaging time
only if \`/icons/icon.png\` actually exists.

For manual invocation:

\`\`\`powershell
python .\tools\phonebuild\main.py --workspace . --project .\projects\gs290-touch-test
python .\tools\phonebuild\main.py --inspect-jar .\projects\gs290-touch-test\dist\GS290TouchTest.jar
\`\`\`

\`--inspect-jar\` checks the JAR manifest, referenced entry class/icon and all
class-file versions; **45.3** is required. CI performs a real compile and
uploads JAR+JAD artifacts.

## Troubleshooting "Not supported" on V10k

1. Inspect both old and new JAR. If the old one reports 52.0,
   the prior compiler was wrong.
2. Install the tiny touch-test MIDlet, **without** optional permissions and
   without proprietary LG libraries. If it works, the AROMA VM basic loader,
   CLDC verification, MIDP Canvas and touch events can be investigated separately.
3. If it fails, report the exact UI message and installation method and compare
   \`META-INF/MANIFEST.MF\`, JAD values, class versions and signer permissions
   against a working on-device JAR. Do not reflash firmware to solve a build issue.
4. The archived LG SDK may provide additional LG-specific UI libraries, but it
   cannot be treated as an exact GS290 profile unless device compatibility is
   verified.

No JAR downloaded from the LG SDK archive was used in these changes; its
directory URL could not be retrieved during this audit.
