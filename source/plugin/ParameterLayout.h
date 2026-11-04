#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace echoform
{

/// Builds the host-facing parameter set from params::kParamSpecs.
juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

/// Human readable value text for a parameter in its natural unit
/// (e.g. "375 ms", "2.50 s", "+3.0 dB", "35 %").
juce::String formatParameterValue (std::size_t specIndex, float value, int maximumStringLength = 0);

} // namespace echoform
