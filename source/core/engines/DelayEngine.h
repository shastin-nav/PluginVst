#pragma once

// Stereo delay engine (milestone P1: Digital and Ping Pong).
//
// Signal flow per channel:
//
//   in ──► (+) ──► softClip ──► [circular buffer] ──► read tap(s) ──► tone ──┬──► wet (width) ──► out
//           ▲                                                                │
//           └──────── × feedback ◄── stereo cross-feed matrix ◄──────────────┘
//
// - Buffers are preallocated in prepare() for kMaxDelaySeconds plus the
//   modulation excursion; process() never allocates.
// - Delay-time changes crossfade between the old and the new read position
//   over kTimeCrossfadeSeconds (no pitch glide, no clicks). Pitch-gliding
//   time changes are a Tape-mode trait (P2).
// - Modulation: a sine LFO (left) and its 90° copy (right) move the read
//   position, read with 4-point cubic interpolation.
// - Tone in the loop: below 0 a low-pass sweeps 20 kHz -> 800 Hz; above 0 a
//   high-pass sweeps 15 Hz -> 1 kHz. A 15 Hz high-pass is always present, so
//   DC can never accumulate in the loop.
// - Saturation: softClip on the written sample is transparent up to 0 dBFS
//   and bounds the buffer to +/-2, so the loop cannot diverge even at 100 %
//   feedback.
// - Cross-feed matrix [[1-x, x], [x, 1-x]] has eigenvalues 1 and 1-2x, so it
//   never increases energy. Digital: x = 0; Ping Pong: x = 1 with the input
//   summed to mono and fed to the left channel only.
//
// Tape and BBD modes are milestone P2; until then they use the Digital path.

#include "dsp/DelayLine.h"
#include "dsp/Filters.h"
#include "dsp/LinearSmoother.h"
#include "dsp/QuadratureOscillator.h"
#include "engines/EngineSettings.h"

#include <array>

namespace echoform::engines
{

class DelayEngine
{
public:
    static constexpr double kMinDelaySeconds = 0.001;
    static constexpr double kMaxDelaySeconds = 8.0;
    static constexpr double kMaxModulationSeconds = 0.003;
    static constexpr double kTimeCrossfadeSeconds = 0.05;
    static constexpr double kSmoothingSeconds = 0.02;
    static constexpr double kBaseHighpassHz = 15.0;

    /// Worst-case |wet output| for any input, by construction: the buffer is
    /// soft-clipped to +/-2; a cubic Hermite read sums |weights| <= 1.25
    /// (-> 2.5); the one-pole high-pass x - lp(x) at most doubles (-> 5);
    /// width 2 maps to 1.5 L - 0.5 R (x2 -> 10). Typical levels are far lower.
    static constexpr float kOutputBound = 10.0f;

    /// Allocates all buffers for `sampleRate` and resets. Not real-time safe.
    void prepare (double sampleRate);

    /// Clears all audio state. The next setSettings() is applied without ramps.
    void reset() noexcept;

    /// Sets new targets (smoothed). Real-time safe.
    void setSettings (const DelaySettings& settings) noexcept;

    /// Produces the wet signal only. Inputs and outputs must not alias.
    void process (const float* inL, const float* inR, float* wetL, float* wetR, int numSamples) noexcept;

    double getSampleRate() const noexcept { return sampleRate; }

    /// Current read delay in samples (excluding modulation), for tests.
    float getCurrentDelaySamples() const noexcept { return currentDelay; }

private:
    float readTap (std::size_t channel, float delaySamples, float modulation) const noexcept;

    double sampleRate = 0.0;
    std::array<dsp::DelayLine, 2> lines;

    float maxModulationSamples = 0.0f;
    float minReadDelay = 2.0f;
    float maxReadDelay = 2.0f;

    // Delay time: crossfade state machine.
    float targetDelay = 0.0f;
    float currentDelay = 0.0f;
    float nextDelay = 0.0f;
    float fadePosition = 0.0f;
    float fadeIncrement = 0.0f;
    bool fading = false;

    dsp::LinearSmoother feedback;
    dsp::LinearSmoother modulationDepth; ///< In samples.
    dsp::LinearSmoother lowpassCoefficient;
    dsp::LinearSmoother highpassCoefficient;
    dsp::LinearSmoother crossFeed; ///< 0 = Digital, 1 = Ping Pong.
    dsp::LinearSmoother width;

    std::array<dsp::OnePoleLowpass, 2> toneLowpass;
    std::array<dsp::OnePoleHighpass, 2> toneHighpass;
    dsp::QuadratureOscillator lfo;

    bool snapOnNextSettings = true;
};

} // namespace echoform::engines
