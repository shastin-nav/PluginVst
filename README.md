# ECHOFORM

A stereo effects processor with independent **Delay** and **Reverb** engines,
for guitarists, producers, and ambient and experimental musicians.

> **Status: milestone P0 (foundation).** The plug-in builds, loads in hosts,
> passes validation, and has its complete parameter and state architecture,
> but the Delay and Reverb engines are not implemented yet: audio passes
> through with only input/output gain applied. See
> [`PROJECT_STATUS.md`](PROJECT_STATUS.md).

## Formats and platforms

| Platform | Format | Status |
|---|---|---|
| Linux (x86_64) | VST3 | See [`PROJECT_STATUS.md`](PROJECT_STATUS.md) |
| Windows (x64) | VST3 | See [`PROJECT_STATUS.md`](PROJECT_STATUS.md) |
| macOS (arm64 + x86_64) | VST3, AU | See [`PROJECT_STATUS.md`](PROJECT_STATUS.md) |

A platform is listed as supported in `PROJECT_STATUS.md` only after a real
build and validation has passed on it.

## Technology

C++17 · JUCE 8.0.17-security (pinned submodule) · CMake ≥ 3.22 · GoogleTest ·
pluginval · Steinberg VST3 validator · GitHub Actions.

## Quick start

```bash
git clone --recurse-submodules --shallow-submodules <repo-url>
cd PluginVst
cmake --preset linux-gcc        # or windows-msvc / macos-xcode
cmake --build --preset linux-gcc
ctest --preset linux-gcc
```

Full instructions: [`docs/BUILDING.md`](docs/BUILDING.md).

## Documentation

- [`PROJECT_STATUS.md`](PROJECT_STATUS.md): milestones, platform status, open decisions
- [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md): layers, parameter model, real-time rules
- [`docs/TEST_RESULTS.md`](docs/TEST_RESULTS.md): recorded test and validation results
- [`docs/DEPENDENCIES_AND_LICENSING.md`](docs/DEPENDENCIES_AND_LICENSING.md): pinned dependencies and licences
- [`docs/BUILDING.md`](docs/BUILDING.md): build and validation instructions

## Licensing

ECHOFORM is built on JUCE, which is dual-licensed under AGPLv3 or a commercial
JUCE licence. **No distribution licence has been chosen yet.** Read
[`docs/DEPENDENCIES_AND_LICENSING.md`](docs/DEPENDENCIES_AND_LICENSING.md)
before distributing any binary.
