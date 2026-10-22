# Optional Java ME APIs in J2ME DevSpace

J2ME profiles continue to compile CLDC 1.1 / MIDP 2.0 using
ECJ 45.3 + StackMap. Applications needing the JSR-75 FileConnection
optional package can now declare in `j2me.proj.json`:

```json
{
  "optional_apis": ["jsr75-file"],
  "optional_permissions": [
    "javax.microedition.io.Connector.file.read",
    "javax.microedition.io.Connector.file.write"
  ]
}
```

Phonebuild downloads MicroEmulator JSR-75 2.0.4 **only as a compile-time
reference** into `sdk/phonebuild/jsr75-file.jar`. It is **never packaged
inside the handset JAR**; the device must actually implement the optional API.

Both requested file permissions are emitted as `MIDlet-Permissions-Opt`
in JAR manifest and JAD so the core MIDlet is still installable if the
phone denies file access. The manifest uses the standard 72-byte line
folding rule. The JAD preserves comma-delimited permissions on a single
logical attribute line.

JSR-75 clients should check availability and handle
`SecurityException`, `IOException` and missing storage roots. The
API is not portable to all MIDP phones.

Existing projects **without** `optional_apis` use the previous
compiler behavior with no additional downloads.
