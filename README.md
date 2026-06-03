# J2ME DevSpace 0.1b-beta `Vector`

[![Build CLI](https://github.com/IlyaBOT/J2ME-DevSpace/actions/workflows/build-cli.yml/badge.svg)](https://github.com/IlyaBOT/J2ME-DevSpace/actions/workflows/build-cli.yml)

Portable CLI workspace manager for Java ME / J2ME MIDP development.

## Build J2ME DevSpace

J2ME DevSpace itself is a native C++ CLI application. The repository contains only the SDK manager source code and templates; actual J2ME workspaces are created later with `j2me --workspace setup`.

Required tools:

- CMake 3.18+
- C++17 compiler
- Git
- Internet access during the first CMake configure step, unless `yaml-cpp` and `nlohmann/json` are already installed locally

### Linux

Ubuntu/Debian:

```bash
sudo apt update
sudo apt install -y build-essential cmake git
```

Build:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
./bin/j2me --version
```

### macOS

Install command line tools and CMake:

```bash
xcode-select --install
brew install cmake git
```

Build:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
./bin/j2me --version
```

### Windows

Recommended setup:

- Visual Studio 2022 Build Tools with the **Desktop development with C++** workload
- CMake
- Git for Windows

With `winget`:

```powershell
winget install Microsoft.VisualStudio.2022.BuildTools
winget install Kitware.CMake
winget install Git.Git
```

Build from PowerShell:

```powershell
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
.\bin\Release\j2me.exe --version
```

Depending on the selected CMake generator, the executable may also appear as:

```powershell
.\bin\j2me.exe --version
```

### GitHub Actions builds

The `Build CLI` workflow builds three artifacts on every push, pull request, and manual run:

```text
j2me-devspace-linux-x64.zip
j2me-devspace-macos-x64.zip
j2me-devspace-windows-x64.zip
```

Successful artifacts are attached to the workflow run. Use them as release candidates when creating a GitHub Release. The macOS artifact is built for x64/Intel because that is the safest baseline for older Macs; add a separate `macos-26` matrix entry later if a native Apple Silicon binary is needed.

J2ME DevSpace keeps the **CLI SDK source tree** separate from the user's **J2ME workspace**. The repository contains only C++ source code, templates, metadata, CI config and documentation. Real J2ME projects, downloaded SDKs, tools, emulators and build artifacts live in a workspace created by `j2me --workspace setup`.

## Repository description

**Portable CLI workspace manager for Java ME / J2ME MIDP development.**

Suggested GitHub topics:

```text
j2me java-me midp cldc developer-tools
```

## Repository layout

```text
J2ME-DevSpace/
├── bin/                 # created by CMake, contains j2me binary
├── src/                 # C++ source code for the CLI
├── templates/           # files copied into new workspaces
│   ├── tools/           # built-in Python tools
│   ├── profiles/        # built-in device profiles
│   ├── packages/        # package index examples
│   └── project/         # reserved project templates
├── j2me.devspace.yml    # CLI metadata: name, version, channel, codename
├── CMakeLists.txt
├── LICENSE
├── THIRD_PARTY_NOTICES.md
├── README.md
└── .gitignore
```

No `projects/`, `sdk/`, `tools/`, `emulators/` directories are stored in the repo root. They are workspace-owned runtime data.

## Metadata

CLI metadata is stored in YAML:

```yaml
schema: j2me.devspace/1
name: J2ME DevSpace
id: j2me-devspace
cli: j2me
version: 0.1b
channel: beta
codename: Vector
description: Portable CLI environment and workspace manager for Java ME / J2ME MIDP projects.
default_profile: lg-gs290
```

`j2me --version` prints:

```text
J2ME DevSpace 0.1b-beta (Vector)
```

## Dependencies

The CLI intentionally uses existing open-source libraries instead of hand-rolled parsers:

- `yaml-cpp` for YAML parsing.
- `nlohmann/json` for JSON parsing and writing.

CMake first tries `find_package(...)`. If packages are not installed, it downloads them with `FetchContent`.

## Workspace commands

Create workspace in current directory:

```bash
j2me --workspace setup
```

Create workspace in a specific directory:

```bash
j2me --workspace setup ~/J2ME-Workspace
cd ~/J2ME-Workspace
```

Show workspace status:

```bash
j2me --workspace info
```

Remove only J2ME DevSpace-owned content from a workspace:

```bash
j2me --workspace remove ~/J2ME-Workspace
```

The remover requires a valid `j2me.workspace.json` marker and removes only the directories created by setup:

```text
projects/
sdk/
emulators/
tools/
profiles/
packages/
cache/
temp/
docs/
j2me.workspace.json
README.workspace.md
```

Other files in the same parent directory are not touched.

## Workspace layout

```text
<workspace>/
├── j2me.workspace.json
├── README.workspace.md
├── projects/
├── sdk/
│   ├── jdk/
│   ├── wtk/
│   ├── ant/
│   └── proguard/
├── emulators/
│   ├── freej2me-plus/
│   └── microemulator/
├── tools/
├── profiles/
├── packages/
├── cache/
├── temp/
└── docs/
```

## Main commands

All commands except `--version`, `--help` and `--workspace setup/remove` require the terminal to be inside a workspace directory tree.

```bash
j2me --project create MyGame
j2me --project list
j2me --project properties MyGame
j2me --sdk setup
j2me --sdk list
j2me --tools list
j2me --tools icongen logo.png --sizes 16,24,32,48,64 --palette 256
j2me --tools audio music.wav --profile music-mp3
j2me --tools palette bg.png --colors 256 --method nearest --dither floyd
j2me --build MyGame --profile lg-gs290 --jdk 8u492 --run-after-build
j2me --emulator setup
j2me --run MyGame
```

## Config files

### Workspace metadata: `j2me.workspace.json`

Created by `j2me --workspace setup`:

```json
{
  "schema": "j2me.workspace/1",
  "name": "J2ME DevSpace Workspace",
  "created_by": {
    "name": "J2ME DevSpace",
    "version": "0.1b",
    "channel": "beta",
    "codename": "Vector"
  },
  "default_profile": "lg-gs290",
  "dirs": {
    "projects": "projects",
    "sdk": "sdk",
    "emulators": "emulators",
    "tools": "tools",
    "profiles": "profiles",
    "packages": "packages",
    "cache": "cache",
    "temp": "temp",
    "docs": "docs"
  }
}
```

### Project metadata: `j2me.proj.json`

Created in `projects/<name>/`:

```json
{
  "schema": "j2me.proj/1",
  "name": "MyGame",
  "midlet_name": "MyGame",
  "midlet_class": "com.ilyabot.MyGame.MainMIDlet",
  "version": "0.1.0",
  "vendor": "IlyaBOT",
  "jar_name": "MyGame.jar",
  "icon": "/icons/icon32.png",
  "permissions": ["javax.microedition.io.Connector.http"],
  "default_profile": "lg-gs290"
}
```

### Profile metadata: `j2me.profile-<name>.json`

The first bundled profile is `lg-gs290`, but it is only the default/reference profile, not the whole point of the SDK.

## Build info

During build, J2ME DevSpace generates:

```text
build/generated/MANIFEST.MF
build/generated-src/<package>/BuildInfo.java
dist/<Project>.jad
```

Manifest/JAD include:

```text
J2ME-DevSpace-Name: J2ME DevSpace
J2ME-DevSpace-Version: 0.1b
J2ME-DevSpace-Channel: beta
J2ME-DevSpace-Codename: Vector
J2ME-Build-Date: <date>
J2ME-Build-Info: <Project> built <date> with J2ME DevSpace 0.1b Vector.
```

MIDlets can read descriptor/manifest properties with `getAppProperty("J2ME-Build-Info")`, and generated code can use `BuildInfo.BUILD_INFO`.

## License

Project code is licensed under Apache License 2.0. See `LICENSE`.

Third-party libraries and tools are listed in `THIRD_PARTY_NOTICES.md`.
