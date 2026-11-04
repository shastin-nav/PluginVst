# ECHOFORM Architecture

Status: describes the code as of milestone P0. Sections marked *(planned)*
describe the design the next milestones implement; they are not yet code.

## Layers

```
┌──────────────────────────────────────────────────────────────────────┐
│ ui/            Editor, theme, custom controls (P2)                   │
│                talks ONLY to AudioProcessorValueTreeState            │
├──────────────────────────────────────────────────────────────────────┤
│ plugin/        JUCE adapter layer                                    │
│   PluginProcessor   bus handling, processBlock orchestration         │
│   ParameterLayout   host parameters generated from the spec table   │
│   ParameterBridge   lock-free host-param -> ParameterSnapshot       │
│   StateSerializer   versioned, deterministic save/load              │
├──────────────────────────────────────────────────────────────────────┤
│ core/          Framework-independent C++17 (library echoform_core)   │
│   params/   ParameterSpecs (single source of truth), Snapshot        │
│   dsp/      LinearSmoother, FloatSafety; engines (P1+)               │
└──────────────────────────────────────────────────────────────────────┘
```

Rules enforced by the build structure:

- `core/` never includes JUCE. It is compiled and tested on its own
  (`EchoformCoreTests` links nothing but `echoform_core` and GoogleTest).
- The editor never holds pointers to DSP objects. It reads and writes
  parameters through the APVTS, exactly as a host does. This keeps the GUI
  decoupled from DSP implementations and off the audio thread.

## Parameter model

`source/core/params/ParameterSpecs.h` holds one `constexpr` table,
`kParamSpecs`, indexed by `enum class Param`. Each entry defines the stable
host ID, display name, kind (float/choice/bool), unit, range, default, skew
centre and step. Everything else is derived from it:

- `plugin/ParameterLayout.cpp` creates the JUCE parameters (with unit-aware
  value text, e.g. `375 ms`, `1.20 s`, `+3.0 dB`, `35 %`).
- `plugin/ParameterBridge` resolves each ID to its `std::atomic<float>` once.
- `ParameterSnapshot` stores one value per `Param` for the DSP.

`static_assert`s validate the table at compile time: order matches the enum,
IDs are unique, `min < max`, defaults and skew centres are in range, choice
lists are present, and tempo divisions are sorted.

Values are stored in natural units (dB, ms, s, Hz, %). Compatibility rules
(IDs are persisted by hosts): never rename/remove an ID after release, never
change a range without a state-schema migration, append new parameters only.
Every ID is registered with `juce::ParameterID { id, 1 }` (version hint for AU).

Current parameter set (31): global `routing`, `inputGain`, `outputGain`,
`macroSpace`, `macroMovement`, `macroInfinite`; delay `delayBypass`,
`delayMode`, `delayTime`, `delaySync`, `delayDivision`, `delayFeedback`,
`delayTone`, `delayMix`, `delayModRate`, `delayModDepth`, `delayWidth`;
reverb `reverbBypass`, `reverbMode`, `reverbDecay`, `reverbSize`,
`reverbPreDelay`, `reverbDamping`, `reverbDiffusion`, `reverbModRate`,
`reverbModDepth`, `reverbWidth`, `reverbFreeze`, `reverbShimmerInterval`,
`reverbShimmerAmount`, `reverbMix`.

In P0 only `inputGain` and `outputGain` affect audio. All other parameters are
declared (so the automation/state contract is fixed early) but are not yet
connected to DSP, and the editor deliberately shows no controls for them.

## Threading and real-time model

| Thread | Does | Never does |
|---|---|---|
| Audio (`processBlock`) | relaxed atomic loads into `ParameterSnapshot`, DSP, smoothing | allocate/free, lock, log, touch files, read the ValueTree |
| Message (UI, state) | APVTS, editor painting, `get/setStateInformation` | touch DSP state directly |

Per block, `processBlock` does:

1. `ScopedNoDenormals` (flush-to-zero).
2. Mono input → duplicated to both outputs; unfed outputs cleared.
3. `ParameterBridge::update()` → snapshot (clamped; NaN/Inf → default).
4. Input gain (20 ms linear ramp) → *[engines, P1]* → output gain (20 ms ramp).
5. NaN/Inf guard: any non-finite output sample is zeroed and counted
   (`getNonFiniteSampleCount()`).

Real-time guarantees are tested, not assumed: the plug-in test executable
replaces global `operator new/delete` and asserts that `processBlock` and
`processBlockBypassed` perform zero heap operations over hundreds of blocks of
varying sizes with live automation.

`prepareToPlay` rebuilds all audio state from current parameter values with no
pending ramps (deterministic initialisation). Any sample rate and any block
size (including 0 and blocks larger than announced) are handled.

## State

`StateSerializer` writes JUCE binary-wrapped XML: root `ECHOFORM_STATE` with
`schemaVersion` and `pluginVersion`, one `PARAM` child per parameter.

Loading is deterministic: a complete state is rebuilt from the spec table,
taking each parameter's value from the blob if present and valid, otherwise
its default. Values are clamped to range, non-finite values rejected, choice
and bool values rounded. Blobs that are not valid ECHOFORM states (garbage,
other products' XML, truncated data) are ignored and leave the state untouched.

## Bus layouts

Main output: stereo only. Main input: mono or stereo. A mono input is fed to
both outputs (common for guitar use). Host bypass passes the dry signal with
the same mono→stereo handling.

## Planned (P1–P2)

- **Delay engine** *(planned)*: preallocated circular buffers sized in
  `prepareToPlay` for the maximum time at the current sample rate, fractional
  (cubic/Thiran) interpolation, feedback path with tone filter, saturation and
  LFO modulation, stereo cross-feedback for Ping Pong; modes differ in signal
  path, not just settings.
- **Reverb engine** *(planned)*: custom FDN with a feedback matrix, multiple
  modulated delay lines, all-pass input diffusion, frequency-dependent damping
  derived from RT60, freeze, and pitch shifting in the Shimmer feedback path.
  Modes use different topologies/line counts, not cosmetic presets.
- **Routing** *(planned)*: Parallel, Delay → Reverb, Reverb → Delay, with
  independent engine mix and bypass.
- **Macros** *(planned)*: SPACE, MOVEMENT, INFINITE map through documented
  (nonlinear where appropriate) curves onto *effective* engine settings,
  computed from the snapshot's base values each block. Base parameter values
  are never overwritten by macros.
