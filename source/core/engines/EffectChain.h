#pragma once

// Routes the Delay and Reverb engines and mixes them with the dry signal.
//
// Each engine is a "stage" with its own equal-power wet/dry mix and bypass:
//
//   stage_k(x) = dry_k * x + wet_k * engine_k(x * active_k)
//   dry_k = sin((1 - m_k) * pi/2),  wet_k = sin(m_k * pi/2)    (enabled)
//   dry_k = 1,                      wet_k = 0                  (bypassed)
//
// Routing:
//   Delay > Reverb:  y = stage_r(stage_d(x))
//   Reverb > Delay:  y = stage_d(stage_r(x))
//   Parallel:        y = dry_d * dry_r * x + wet_d * delay(x) + wet_r * reverb(x)
//
// All gains are smoothed (20 ms). A bypassed engine keeps running with its
// input faded to silence, so its tail decays naturally, CPU load stays
// constant, and re-enabling it never replays stale audio. A routing change
// fades the output out (10 ms), switches, and fades back in (10 ms).
//
// With both mixes at 0 (or both engines bypassed) the chain is bit-transparent.

#include "dsp/LinearSmoother.h"
#include "engines/DelayEngine.h"
#include "engines/EngineSettings.h"
#include "engines/FdnReverb.h"

#include <vector>

namespace echoform::engines
{

class EffectChain
{
public:
    static constexpr int kBlockSize = 256;
    static constexpr double kGainSmoothingSeconds = 0.02;
    static constexpr double kRoutingFadeSeconds = 0.01;

    /// Allocates everything for `sampleRate`. Not real-time safe.
    void prepare (double sampleRate);

    /// Clears all audio state; the next setSettings() applies without ramps.
    void reset() noexcept;

    void setSettings (const ChainSettings& settings) noexcept;

    /// In-place stereo processing; any block length.
    void process (float* left, float* right, int numSamples) noexcept;

    /// Runs the engines on silence (used while the host bypasses the plug-in)
    /// so tails keep decaying instead of resuming later.
    void processSilence (int numSamples) noexcept;

    params::Routing getActiveRouting() const noexcept { return activeRouting; }

    DelayEngine& getDelay() noexcept { return delay; }
    FdnReverb& getReverb() noexcept { return reverb; }

private:
    struct StageGains
    {
        dsp::LinearSmoother active; ///< Engine input gain (0 when bypassed).
        dsp::LinearSmoother dry;
        dsp::LinearSmoother wet;
    };

    void processChunk (float* left, float* right, int numSamples) noexcept;
    void runStage (StageGains& gains, bool isDelay, float* left, float* right, int numSamples) noexcept;
    void runEngine (bool isDelay, const float* inL, const float* inR, float* wetL, float* wetR, int numSamples) noexcept;
    static void setStageTargets (StageGains& gains, bool enabled, float mix) noexcept;

    double sampleRate = 0.0;
    DelayEngine delay;
    FdnReverb reverb;

    StageGains delayGains;
    StageGains reverbGains;

    params::Routing activeRouting = params::Routing::DelayToReverb;
    params::Routing requestedRouting = params::Routing::DelayToReverb;
    dsp::LinearSmoother routingGain;

    std::vector<float> scratch; ///< 6 * kBlockSize samples, allocated in prepare().
    bool snapOnNextSettings = true;
};

} // namespace echoform::engines
