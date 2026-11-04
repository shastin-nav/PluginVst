#pragma once

#include "plugin/PluginProcessor.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <memory>
#include <random>
#include <stdexcept>
#include <string>

namespace echoform::test
{

inline std::unique_ptr<EchoformAudioProcessor> makePreparedProcessor (double sampleRate = 48000.0, int blockSize = 512)
{
    auto processor = std::make_unique<EchoformAudioProcessor>();
    processor->setRateAndBufferSizeDetails (sampleRate, blockSize);
    processor->prepareToPlay (sampleRate, blockSize);
    return processor;
}

/// Sets both engine mixes to 0 (engines run, output is exactly dry).
/// Must be applied before prepareToPlay(), which snaps all smoothing.
inline void setEngineMixesToZero (EchoformAudioProcessor& p);

/// A prepared processor whose engines are mixed out: bit-transparent at 0 dB.
inline std::unique_ptr<EchoformAudioProcessor> makeTransparentProcessor (double sampleRate = 48000.0, int blockSize = 512)
{
    auto processor = std::make_unique<EchoformAudioProcessor>();
    setEngineMixesToZero (*processor);
    processor->setRateAndBufferSizeDetails (sampleRate, blockSize);
    processor->prepareToPlay (sampleRate, blockSize);
    return processor;
}

/// Deterministic white noise in [-0.5, 0.5].
inline void fillWithNoise (juce::AudioBuffer<float>& buffer, unsigned seed)
{
    std::mt19937 rng { seed };
    std::uniform_real_distribution<float> dist { -0.5f, 0.5f };
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        for (int i = 0; i < buffer.getNumSamples(); ++i)
            buffer.setSample (ch, i, dist (rng));
}

inline juce::RangedAudioParameter& parameter (EchoformAudioProcessor& p, const char* id)
{
    auto* param = p.getValueTreeState().getParameter (id);
    if (param == nullptr)
        throw std::runtime_error (std::string ("no parameter ") + id);
    return *param;
}

/// Sets a parameter to a value in its natural unit, as a host would.
inline void setParameterValue (EchoformAudioProcessor& p, const char* id, float value)
{
    auto& param = parameter (p, id);
    param.setValueNotifyingHost (param.convertTo0to1 (value));
}

inline float getParameterValue (EchoformAudioProcessor& p, const char* id)
{
    auto& param = parameter (p, id);
    return param.convertFrom0to1 (param.getValue());
}

inline void setEngineMixesToZero (EchoformAudioProcessor& p)
{
    setParameterValue (p, "delayMix", 0.0f);
    setParameterValue (p, "reverbMix", 0.0f);
}

/// Host play head reporting a fixed tempo.
struct FixedTempoPlayHead final : juce::AudioPlayHead
{
    explicit FixedTempoPlayHead (double tempo) : bpm (tempo) {}

    juce::Optional<PositionInfo> getPosition() const override
    {
        PositionInfo info;
        info.setBpm (bpm);
        info.setIsPlaying (true);
        return info;
    }

    double bpm;
};

} // namespace echoform::test
