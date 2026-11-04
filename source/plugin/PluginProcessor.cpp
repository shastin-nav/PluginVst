#include "plugin/PluginProcessor.h"

#include "dsp/FloatSafety.h"
#include "plugin/ParameterLayout.h"
#include "plugin/PluginEditor.h"
#include "plugin/StateSerializer.h"

namespace echoform
{
namespace
{
    float decibelsToGain (float db) noexcept { return juce::Decibels::decibelsToGain (db, -100.0f); }
} // namespace

EchoformAudioProcessor::EchoformAudioProcessor()
    : juce::AudioProcessor (BusesProperties()
                                .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      state (*this, nullptr, juce::Identifier (state::kStateType), createParameterLayout()),
      parameterBridge (state)
{
}

// =================================================================================================
// Lifecycle

void EchoformAudioProcessor::prepareToPlay (double sampleRate, int /*maximumExpectedSamplesPerBlock*/)
{
    // Deterministic initialisation: every piece of audio state is rebuilt from
    // the current parameter values, with no ramps pending.
    parameterBridge.update (snapshot);

    inputGain.reset (sampleRate, kGainSmoothingSeconds, decibelsToGain (snapshot.get (params::Param::inputGain)));
    outputGain.reset (sampleRate, kGainSmoothingSeconds, decibelsToGain (snapshot.get (params::Param::outputGain)));
}

void EchoformAudioProcessor::releaseResources() {}

bool EchoformAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto output = layouts.getMainOutputChannelSet();
    const auto input = layouts.getMainInputChannelSet();

    if (output != juce::AudioChannelSet::stereo())
        return false;

    return input == juce::AudioChannelSet::mono() || input == juce::AudioChannelSet::stereo();
}

// =================================================================================================
// Audio

void EchoformAudioProcessor::prepareStereoChannels (juce::AudioBuffer<float>& buffer) const noexcept
{
    const auto numSamples = buffer.getNumSamples();
    const auto numInputs = getTotalNumInputChannels();
    const auto numOutputs = juce::jmin (getTotalNumOutputChannels(), buffer.getNumChannels());

    if (numInputs == 1 && numOutputs >= 2)
        buffer.copyFrom (1, 0, buffer, 0, 0, numSamples);

    const auto firstUnfed = numInputs == 1 ? 2 : numInputs;
    for (auto channel = firstUnfed; channel < numOutputs; ++channel)
        buffer.clear (channel, 0, numSamples);
}

void EchoformAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ignoreUnused (midi);
    juce::ScopedNoDenormals noDenormals;

    const auto numSamples = buffer.getNumSamples();
    if (numSamples <= 0)
        return;

    prepareStereoChannels (buffer);

    parameterBridge.update (snapshot);
    inputGain.setTarget (decibelsToGain (snapshot.get (params::Param::inputGain)));
    outputGain.setTarget (decibelsToGain (snapshot.get (params::Param::outputGain)));

    const auto numChannels = juce::jmin (2, buffer.getNumChannels());
    auto* left = buffer.getWritePointer (0);
    auto* right = numChannels > 1 ? buffer.getWritePointer (1) : nullptr;

    // Delay and Reverb engines are inserted between these two gain stages in P1.
    for (int i = 0; i < numSamples; ++i)
    {
        const auto gain = inputGain.next() * outputGain.next();
        left[i] *= gain;
        if (right != nullptr)
            right[i] *= gain;
    }

    // Last line of defence: never hand NaN/Inf to the host.
    auto replaced = dsp::sanitize (left, numSamples);
    if (right != nullptr)
        replaced += dsp::sanitize (right, numSamples);

    if (replaced > 0)
        nonFiniteSamples.fetch_add (static_cast<std::uint64_t> (replaced), std::memory_order_relaxed);
}

void EchoformAudioProcessor::processBlockBypassed (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ignoreUnused (midi);

    // Host bypass: dry signal, but still a valid stereo image for mono inputs.
    if (buffer.getNumSamples() > 0)
        prepareStereoChannels (buffer);
}

// =================================================================================================
// Editor

juce::AudioProcessorEditor* EchoformAudioProcessor::createEditor()
{
    return new EchoformAudioProcessorEditor (*this);
}

// =================================================================================================
// State

void EchoformAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    state::write (state, JucePlugin_VersionString, destData);
}

void EchoformAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    state::read (state, data, sizeInBytes);
}

} // namespace echoform

// =================================================================================================
// Plug-in entry point required by the JUCE plug-in wrappers.

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new echoform::EchoformAudioProcessor();
}
