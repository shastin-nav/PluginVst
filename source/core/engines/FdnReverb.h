#pragma once

// Stereo feedback-delay-network reverb (milestone P1: one late-reverb topology).
//
//   in ─► DC block ─► pre-delay ─► 4 all-pass diffusers (per channel) ─► × inputGain ─┐
//                                                                                     ▼
//        ┌──────────── 16 modulated delay lines (cubic reads) ◄──────────────────── (+)
//        │                    │                                                    ▲
//        │                    ├──► output taps (orthogonal ±1 patterns) ─► width ─► wet
//        ▼                                                                         │
//   damping (one-pole per line) ─► × g_i ─► normalised Hadamard (orthogonal) ──────┘
//
// Decay: line i with length m_i samples gets gain g_i = 10^(-3 m_i / (fs T60)),
// so every path loses exactly 60 dB per T60 at low frequencies, independent of
// the line length (Jot). With an orthogonal matrix the whole network then
// decays at the requested RT60.
//
// Frequency-dependent damping: a one-pole low-pass per line whose DC gain is 1
// and whose Nyquist gain is r_i = (g_hf_i / g_i), where g_hf_i is the gain
// needed for T60_hf = alpha * T60 (alpha from the damping control). The
// coefficient b_i = (1 - r_i) / (1 + r_i) makes both band edges exact.
//
// Freeze: input to the tank fades out, damping and modulation fade to zero,
// each line glides to an integer length (a fractional cubic read would
// low-pass every pass and drain the tail) and the loop gain fades to
// kFreezeLoopGain. Cubic Hermite reads never exceed unity gain and the
// Hadamard matrix is orthogonal, so the frozen network sustains without
// growing; kFreezeLoopGain (just below 1) guards against float rounding in the
// matrix (about 0.1 dB of decay per minute).
//
// Reverb modes (Room/Hall/Plate/Ambient/Shimmer) are milestone P2; until then
// every mode uses this topology.

#include "dsp/Allpass.h"
#include "dsp/DelayLine.h"
#include "dsp/Filters.h"
#include "dsp/LinearSmoother.h"
#include "dsp/QuadratureOscillator.h"
#include "engines/EngineSettings.h"

#include <array>
#include <cstddef>

namespace echoform::engines
{

class FdnReverb
{
public:
    static constexpr std::size_t kNumLines = 16;
    static constexpr std::size_t kNumDiffusers = 4;
    static constexpr int kControlBlockSize = 32;

    static constexpr double kMaxPreDelaySeconds = 0.25;
    static constexpr double kMinSizeScale = 0.4;
    static constexpr double kMaxSizeScale = 1.6;
    static constexpr double kMaxModulationSeconds = 0.0008;
    static constexpr double kMaxDiffusionGain = 0.75;
    static constexpr double kMinDampingRatio = 0.1; ///< T60_hf / T60 at 100 % damping.
    static constexpr float kFreezeLoopGain = 0.999995f;
    static constexpr float kOutputGain = 0.5f;

    static constexpr double kSizeSmoothingSeconds = 0.25;
    static constexpr double kParameterSmoothingSeconds = 0.05;
    static constexpr double kFreezeRampSeconds = 0.1;

    /// Base line lengths (seconds) at size scale 1.0.
    static const std::array<double, kNumLines>& baseLineSeconds() noexcept;

    void prepare (double sampleRate);
    void reset() noexcept;
    void setSettings (const ReverbSettings& settings) noexcept;

    /// Produces the wet signal only. Inputs and outputs must not alias.
    void process (const float* inL, const float* inR, float* wetL, float* wetR, int numSamples) noexcept;

    double getSampleRate() const noexcept { return sampleRate; }

private:
    void updateCoefficients() noexcept;
    void processSample (float inL, float inR, float& outL, float& outR) noexcept;

    double sampleRate = 0.0;

    // Input stage
    std::array<dsp::DcBlocker, 2> dcBlockers;
    std::array<dsp::DelayLine, 2> preDelay;
    std::array<std::array<dsp::SchroederAllpass, kNumDiffusers>, 2> diffusers;

    // Tank
    std::array<dsp::DelayLine, kNumLines> lines;
    std::array<dsp::OnePoleLowpass, kNumLines> damping;
    std::array<float, kNumLines> baseLengthSamples {};
    std::array<float, kNumLines> loopGains {};
    std::array<float, kNumLines> modulationCos {};
    std::array<float, kNumLines> modulationSin {};
    dsp::QuadratureOscillator lfo;
    float maxModulationSamples = 0.0f;
    float maxReadDelay = 2.0f;

    // Smoothed controls
    dsp::LinearSmoother sizeScale;          // per sample
    dsp::LinearSmoother preDelaySamples;    // per sample
    dsp::LinearSmoother modulationDepth;    // per sample, in samples
    dsp::LinearSmoother width;              // per sample
    dsp::LinearSmoother decaySeconds;       // per control block
    dsp::LinearSmoother dampingRatio;       // per control block
    dsp::LinearSmoother diffusionGain;      // per control block
    dsp::LinearSmoother freezeAmount;       // per control block

    float tankInputGain = 1.0f;
    float currentFreeze = 0.0f;
    bool snapOnNextSettings = true;
};

} // namespace echoform::engines
