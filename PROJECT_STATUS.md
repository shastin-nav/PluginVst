# ECHOFORM: Project Status

**Last updated:** 2026-11-04
**Current milestone:** P0 (foundation) complete
**Version:** 0.1.0 (pre-release, internal only; see Licensing below)

## Platform support

A platform is listed as supported only after a real build **and** validation
passed on it.

| Platform | Format | Build | Unit tests | Host validation | Status |
|---|---|---|---|---|---|
| Linux x86_64 (GCC 13.3 / Clang 18.1) | VST3 | Pass (local + CI) | 49/49 | pluginval s10 SUCCESS; Steinberg validator 47/47 + 537/537 | **Supported** |
| Windows x64 (MSVC 19.44, VS 2022) | VST3 | Pass (CI) | 49/49 (CI) | pluginval s10 SUCCESS; Steinberg validator 47/47 + 537/537 (CI) | **Supported** |
| macOS universal arm64 + x86_64 (Xcode 16.4) | VST3 + AU | Pass (CI) | 49/49 (CI, on arm64) | `auval -strict` SUCCEEDED; pluginval s10 SUCCESS on VST3 and AU; Steinberg validator 47/47 + 537/537 (CI) | **Supported** |

Verified by CI run [#3](https://github.com/shastin-nav/PluginVst/actions/runs/41792266926) on commit `a9ab75e`.
The x86_64 slice of the macOS universal binary is built but has not been run (the CI runner is Apple Silicon).

Details and links to CI runs: [`docs/TEST_RESULTS.md`](docs/TEST_RESULTS.md).

## Milestones

### P0: Foundation (complete)

- [x] Environment audit ([`docs/ENVIRONMENT_AUDIT.md`](docs/ENVIRONMENT_AUDIT.md))
- [x] Project structure, CMake ≥ 3.22, presets for Linux (GCC/Clang), Windows (MSVC), macOS (Xcode)
- [x] JUCE 8.0.17-security pinned as a git submodule; licence read ([`docs/DEPENDENCIES_AND_LICENSING.md`](docs/DEPENDENCIES_AND_LICENSING.md))
- [x] Minimal VST3 (+ AU on macOS) that builds, loads and validates
- [x] Parameter architecture: 31 parameters from one compile-time-validated spec table; lock-free bridge to DSP snapshot
- [x] State serialisation: versioned schema, deterministic loading, corrupt-input safe
- [x] Real-time skeleton: smoothed in/out gain, NaN/Inf guard, mono → stereo, any block size and sample rate
- [x] GoogleTest suites (49 tests), including a zero-heap-allocation check of `processBlock`
- [x] Host validation: pluginval strictness 10 and Steinberg's VST3 validator
- [x] GitHub Actions: independent Linux, Windows, macOS runners

### P1: Core DSP (next)

- [ ] Digital Delay: preallocated circular buffer, fractional interpolation, feedback with tone filter, tempo sync, smoothing
- [ ] FDN Reverb: feedback matrix, multiple modulated delay lines, all-pass diffusion, frequency-dependent damping, RT60 control, freeze
- [ ] Stereo mixer and routing (Parallel, Delay → Reverb, Reverb → Delay); per-engine mix and bypass
- [ ] Tail length reporting
- [ ] DSP tests: delay timing, feedback stability, RT60 measurement, decay, stereo separation, freeze, smoothing, numerical stability

### P2: Character and interface

- [ ] Tape, BBD, Ping Pong delay modes (distinct signal paths)
- [ ] Room, Hall, Plate, Ambient, Shimmer reverb modes (distinct FDN configurations; pitch shifting for Shimmer)
- [ ] Macros SPACE / MOVEMENT / INFINITE (documented nonlinear mappings; base values preserved)
- [ ] Custom analog-inspired UI: rotary controls, stereo/delay visualisation, reverb envelope, preset browser
- [ ] Preset system

### P3: Release

- [ ] Advanced DSP refinements
- [ ] Release packaging, code signing, notarisation, third-party notices
- [ ] User documentation

## What works now

- Loads in hosts as "Echoform" (VST3 category Fx|Delay|Reverb; AU `aufx Efrm Ecfm`).
- Stereo in/out or mono in → stereo out.
- Input and output gain (smoothed). Everything else passes audio through unchanged.
- All 31 parameters are automatable and saved/restored with the session.
- Editor: on-brand frame showing that the engines are not yet active. It has
  no controls yet, by design (every control must work).

## Known limitations

- Delay and Reverb engines are **not implemented**. Their parameters exist and
  persist but do not affect audio yet.
- `getTailLengthSeconds()` returns 0 (correct for P0; becomes non-zero with the engines).
- Plug-in identity (company name, bundle ID, manufacturer/plug-in codes) is
  provisional and must be finalised before any public release.
- No code signing or notarisation yet; CI only ad-hoc signs for validation.
- Validated in pluginval (JUCE-based host) and Steinberg's validator, not yet
  in a commercial DAW.

## Open decisions (owner action required)

1. **Distribution licence.** JUCE is AGPLv3 or commercial. The JUCE 8 EULA
   could not be retrieved from this environment, so no licence tier, revenue
   limit, or student/educational eligibility has been assumed. Before
   distributing any binary, choose AGPLv3 (publish ECHOFORM's source) or obtain
   the appropriate commercial JUCE licence after reading the EULA.
   See [`docs/DEPENDENCIES_AND_LICENSING.md`](docs/DEPENDENCIES_AND_LICENSING.md).
2. **Plug-in identity.** Company name, bundle ID and the codes `Ecfm`/`Efrm`.
   Hosts store these in sessions; changing them after release breaks recall.
3. **JUCE major version.** JUCE 9.0.x releases now exist. The directive
   specifies JUCE 8; staying on 8 unless instructed otherwise.

## History

| Date | Milestone | Commit |
|---|---|---|
| 2026-11-04 | First VST3 built on Linux | `7744a30` |
| 2026-11-04 | Tests (45) + pluginval + Steinberg validator passing on Linux | `cc7808e` |
| 2026-11-04 | CI on three platforms; Linux and Windows green | `e52eaaa` |
| 2026-11-04 | Clang/AppleClang fixes; docs | `0984680` |
| 2026-11-04 | ARM bit-transparency fix; **all three platforms green in CI** | `a9ab75e` |
