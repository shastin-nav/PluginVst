#pragma once

// Lock-free bridge between host parameters and the DSP layer.
//
// Construction (message thread) resolves every parameter ID to the atomic
// value owned by the AudioProcessorValueTreeState. On the audio thread,
// update() performs only relaxed atomic loads into a ParameterSnapshot:
// no locks, no allocation, no string lookups.

#include "params/ParameterSnapshot.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <array>
#include <atomic>

namespace echoform
{

class ParameterBridge
{
public:
    explicit ParameterBridge (juce::AudioProcessorValueTreeState& state);

    /// Audio thread. Copies current parameter values into `snapshot`.
    void update (params::ParameterSnapshot& snapshot) const noexcept;

    /// The raw atomic for a parameter (never null).
    const std::atomic<float>& raw (params::Param p) const noexcept { return *sources[params::indexOf (p)]; }

private:
    std::array<const std::atomic<float>*, params::kNumParams> sources {};
};

} // namespace echoform
