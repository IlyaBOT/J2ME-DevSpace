# J2ME DevSpace Workspace

This directory is a portable workspace created by `j2me --workspace setup`.

Typical layout:

```text
projects/    J2ME projects
sdk/         JDK, WTK / Java ME SDK, Ant, ProGuard packages
emulators/   FreeJ2ME-Plus, MicroEmulator, WTK emulator wrappers
tools/       Python-based asset tools
profiles/    Device profiles
packages/    Package index files
cache/       Download/update cache
temp/        Safe temporary backups and partial installs
```

Default project:

```text
projects/j2me_devspace-demo/
```

Use it to test build tools, emulator setup and basic MIDP LCDUI widgets on small screens.

Refresh workspace-owned templates after upgrading J2ME DevSpace:

```bash
j2me --workspace update
```

Do not delete `j2me.workspace.json` unless you intentionally want this directory to stop being detected as a workspace.
