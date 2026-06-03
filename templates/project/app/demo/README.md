# J2ME DevSpace Demo

Default demo project copied into every new workspace.

It uses only standard MIDP LCDUI widgets, so small devices can scroll the form instead of clipping content.

Layout:

```text
j2me.proj.json
j2me.profile-lg-gs290.json
res/icons/
src/DemoMIDlet.java
```

Put the demo icon at `res/icons/icon32.png`. It is referenced in `j2me.proj.json` as `/icons/icon32.png`, because the build packs `res/` into the JAR root.

The demo MIDlet intentionally uses the default Java package to keep the template easy to inspect. Real projects can still use Java packages such as `com.example.game`, in which case the source path should match the package path.
