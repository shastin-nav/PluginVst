# Building ECHOFORM

## Requirements

- CMake ≥ 3.22, a C++17 compiler, Git.
- Clone with the JUCE submodule:

  ```bash
  git clone --recurse-submodules --shallow-submodules <repo-url>
  # or, in an existing clone:
  git submodule update --init --depth 1 libs/JUCE
  ```

GoogleTest is downloaded automatically at configure time (pinned by SHA-256)
when `ECHOFORM_BUILD_TESTS=ON` (the default).

## Presets

| Preset | Platform | Generator | Output |
|---|---|---|---|
| `linux-gcc` | Linux | Ninja, Release, GCC | VST3 |
| `linux-gcc-debug` | Linux | Ninja, Debug, GCC | VST3 |
| `linux-clang` | Linux | Ninja, Release, Clang | VST3 |
| `windows-msvc` | Windows | newest installed Visual Studio, x64 | VST3 |
| `macos-xcode` | macOS | Xcode, universal arm64 + x86_64 | VST3 + AU |

```bash
cmake --preset <preset>
cmake --build --preset <preset>
ctest --preset <preset>
```

Artefacts: `build/<preset>/Echoform_artefacts/<Config>/VST3/Echoform.vst3`
(and `.../AU/Echoform.component` on macOS).

Options: `-DECHOFORM_WARNINGS_AS_ERRORS=ON` (used by CI),
`-DECHOFORM_COPY_PLUGIN_AFTER_BUILD=ON` (install into the user plug-in folders),
`-DECHOFORM_BUILD_TESTS=OFF`.

## Linux packages (Ubuntu 24.04)

```bash
sudo apt-get install ninja-build pkg-config \
  libfreetype-dev libfontconfig1-dev \
  libx11-dev libxcomposite-dev libxcursor-dev libxext-dev \
  libxinerama-dev libxrandr-dev libxrender-dev \
  xvfb   # only for running pluginval's editor tests headless
```

WebKit/GTK and libcurl are not needed (`JUCE_WEB_BROWSER=0`, `JUCE_USE_CURL=0`).
This is the package list used by the CI Linux job.

## Validation

```bash
scripts/fetch_pluginval.sh            # pinned pluginval, SHA-256 verified
xvfb-run -a scripts/run_pluginval.sh "$PWD/build/linux-gcc/Echoform_artefacts/Release/VST3/Echoform.vst3" 10

scripts/build_vst3_validator.sh       # Steinberg validator from pinned VST3 SDK
scripts/run_vst3_validator.sh "$PWD/build/linux-gcc/Echoform_artefacts/Release/VST3/Echoform.vst3"
```

Logs are written to `validation-logs/`. Tools live in the git-ignored `tools/`.

On macOS, AU validation additionally uses Apple's tool:
`auval -strict -v aufx Efrm Ecfm` (after installing the component into
`~/Library/Audio/Plug-Ins/Components`).
