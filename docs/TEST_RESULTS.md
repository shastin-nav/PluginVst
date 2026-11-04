# Test Results

Every result below was produced by an actual run. Nothing is extrapolated.
Logs from CI runs are attached to each run as `validation-logs-<platform>`
artefacts.

## Milestone P0, final commit `a9ab75e` (2026-11-04)

### GitHub Actions run #3: all platforms green

Run: <https://github.com/shastin-nav/PluginVst/actions/runs/41792266926>
(every step on every runner succeeded).

| Platform | Toolchain | Build (`-Werror`) | Unit tests | pluginval v1.0.4 s10 | Steinberg validator 3.8.0 | Apple auval |
|---|---|---|---|---|---|---|
| Linux x86_64, `ubuntu-24.04` | GCC 13.3.0, Ninja | Pass | **49/49** | VST3 SUCCESS | 47/0 standard, 537/0 extensive | n/a |
| Windows x64, `windows-2022` | MSVC 19.44 (VS 2022), SDK 10.0.26100 | Pass | **49/49** | VST3 SUCCESS | 47/0 standard, 537/0 extensive | n/a |
| macOS, `macos-15-arm64` | Xcode 16.4, AppleClang 17, universal (arm64 + x86_64, confirmed with `lipo`) | Pass, 0 warnings | **49/49** | VST3 SUCCESS, AU SUCCESS | 47/0 standard, 537/0 extensive | `auval -strict`: **AU VALIDATION SUCCEEDED** |

The only compiler warnings in the CI logs come from Steinberg's SDK sources
while building the validator tool (`-Wformat` in `fstring.cpp`/`ustring.cpp`)
and from GCC's informational LTO notice; none are in ECHOFORM code.

### Local (development container: Ubuntu 24.04, x86_64)

| Check | Configuration | Commit | Result |
|---|---|---|---|
| Build, `-Werror` on project sources | `linux-gcc` (GCC 13.3.0, Release, LTO) | `a9ab75e` | Pass, 0 warnings |
| Build, `-Werror` on project sources | `linux-clang` (Clang 18.1.3, Release, LTO) | `a9ab75e` | Pass, 0 warnings |
| Unit tests | `linux-gcc` | `a9ab75e` | **49/49 pass** (24 core + 25 plug-in) |
| Unit tests | `linux-clang` | `a9ab75e` | **49/49 pass** |
| Unit tests, forced FMA contraction (`-mfma -ffp-contract=fast`) | GCC 13.3.0, Release | `a9ab75e` | **49/49 pass** |
| pluginval v1.0.4, strictness 10, under Xvfb | Release VST3 | `a9ab75e` | **SUCCESS** (exit 0, 25 test groups) |
| Steinberg VST3 validator 3.8.0, standard / extensive (`-e`) | Release VST3 | `a9ab75e` | **47/0**, **537/0** |
| Build + unit tests with JUCE assertions and leak detector | `linux-gcc-debug` | `cc7808e` | 45/45 pass; no assertion or leak output |
| pluginval s10 + Steinberg validator | Debug VST3 (assertions live) | `cc7808e` | SUCCESS; 47/0 and 537/0; no assertions |

pluginval test groups run: scan, open (cold/warm), plugin info, programs,
editor, editor whilst processing, audio processing (sample rates 44.1, 48 and
96 kHz × block sizes 64 to 1024), non-releasing processing, state, state
restoration, automation, editor automation, automatable parameters, parameters,
background-thread state, parameter thread safety, buses (list/enable/disable/
restore), parameter fuzzing.

### CI history

| Run | Commit | Linux | Windows | macOS |
|---|---|---|---|---|
| [#1](https://github.com/shastin-nav/PluginVst/actions/runs/41788865664) | `e52eaaa` | Pass | Pass | **Build failed**: AppleClang `-Wdouble-promotion` in `StateSerializer.cpp` |
| [#2](https://github.com/shastin-nav/PluginVst/actions/runs/41790477574) | `0984680` | Pass | Pass | Build passed; **5 unit tests failed** (0 dB not bit-transparent on arm64, see below) |
| [#3](https://github.com/shastin-nav/PluginVst/actions/runs/41792266926) | `a9ab75e` | Pass | Pass | **Pass** (all steps incl. auval and AU) |

## Defects found by testing and validation (all fixed)

| Found by | Defect | Fix |
|---|---|---|
| Steinberg VST3 validator | "Programlist 000 -> Program 000: has no name" (46 passed, 1 failed) | Host-visible program named "Default" |
| Unit test `OutputGainIsSmoothedThenExact` | Accumulated float error made the final smoother step ~3% larger than the others | Ramp values computed as `start + increment * n` |
| macOS CI (AppleClang 17) | Implicit `float → double` into `juce::var` | Explicit conversion |
| Clang 18 build | Sized `operator delete` replacements undeclared without sized deallocation (GCC requires them) | Defined only when `__cpp_sized_deallocation` is set |
| macOS CI unit tests (Apple Silicon), run #2 | At 0 dB the plug-in was not bit-transparent on arm64 (5 tests; e.g. `-0.278006822` became `-0.278006852`). Cause: 0 dB in the −48…+12 dB range normalises to 0.8 (not representable); converting back, ARM fuses `-48 + 0.8f*60` into one FMA, leaving +7.15e-7 dB, while x86 rounds the product and gets exactly 0. Reproduced numerically and on x86 with forced FMA contraction (`-mfma -ffp-contract=fast`). | `dsp::decibelsToGain` treats \|dB\| < 0.001 as exactly unity (0.0115 % amplitude, inaudible); 4 new core tests |

## Test-harness issues found and fixed (not product defects)

- Allocation-tracker positive control: GCC elided a `new`/`delete` pair (legal
  since C++14), so the control now publishes the pointer through a `volatile`.
- State comparisons used raw normalised floats; JUCE bool parameters keep the
  raw value (e.g. 0.79 = "on"), so tests compare values in natural units.
- Editor tests now follow the host protocol (`editorBeingDeleted()` before
  destroying the editor).

## Verifying the tests themselves

- **Mutation check (real-time safety):** a heap allocation was temporarily
  inserted into `processBlock`. `RealtimeSafety.ProcessBlockNeverTouchesTheHeap`
  failed with "allocation in processBlock, block 0", proving the detector works
  across the plug-in library under LTO. The mutation was reverted.

## Tool interoperability note

pluginval's `--vst3validator` option passes the inner Linux shared object
(`Echoform.vst3/Contents/x86_64-linux/Echoform.so`) to Steinberg's validator,
which rejects it ("is not a module directory", exit 255). Run on the `.vst3`
bundle path, the same validator passes. Reproduced independently of pluginval.
The validator is therefore run as a separate step (`scripts/run_vst3_validator.sh`).

## Test inventory

**EchoformCoreTests** (no JUCE): FloatSafety (finite/non-finite
classification, sanitising, empty input); Gain (0 dB exactly unity, ARM FMA
residue exactly unity, normal conversion outside tolerance, monotonic); LinearSmoother (reset, exact landing
on target, monotonic bounded steps, retargeting, zero-length ramp, skip,
sample-rate scaling); ParameterSpecs (unique host-safe IDs, enum order, ranges
and defaults, choice lists, tempo divisions); ParameterSnapshot (defaults,
clamping, NaN/Inf rejection, choice/bool accessors).

**EchoformPluginTests**: ParameterLayout (exposes exactly the spec table in
order, ranges/defaults, skew centres, bridge reflects every parameter, value
text and parsing); State (round trip, schema/version, missing params → defaults,
out-of-range/NaN sanitising, invalid blobs ignored, deterministic loading);
Processing (bit transparency, mono → stereo, supported layouts, output-gain
smoothing then exact, arbitrary block sizes incl. 0 samples, 0 channels and
larger than prepared, sample-rate
changes 22.05–192 kHz, NaN/Inf guard, bypass, deterministic initialisation);
RealtimeSafety (tracker positive control, zero heap operations in
`processBlock` and `processBlockBypassed`); Editor (size, theme pixel,
open/close while processing).

### Not yet testable (no engines in P0)

Delay timing, feedback stability, reverb decay / RT60, stereo separation,
freeze behaviour: required by the directive, they arrive with the engines in P1.
