#include "plugin/PluginProcessor.h"

#include "dsp/FloatSafety.h"
#include "dsp/Gain.h"
#include "engines/ParameterMapping.h"
#include "plugin/ParameterLayout.h"
#include "plugin/PluginEditor.h"
#include "plugin/StateSerializer.h"

namespace echoform
{
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

    inputGain.reset (sampleRate, kGainSmoothingSeconds, dsp::decibelsToGain (snapshot.get (params::Param::inputGain)));
    outputGain.reset (sampleRate, kGainSmoothingSeconds, dsp::decibelsToGain (snapshot.get (params::Param::outputGain)));

    chain.prepare (sampleRate); // allocates all engine buffers
    chain.setSettings (engines::mapToChainSettings (snapshot, hostBpm.load (std::memory_order_relaxed)));
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
    if (numSamples <= 0 || buffer.getNumChannels() <= 0)
        return;

    prepareStereoChannels (buffer);

    const auto numChannels = juce::jmin (2, buffer.getNumChannels());
    auto* left = buffer.getWritePointer (0);
    auto* right = numChannels > 1 ? buffer.getWritePointer (1) : nullptr;

    // NaN/Inf from the host never enters the engines' feedback loops.
    auto replaced = dsp::sanitize (left, numSamples);
    if (right != nullptr)
        replaced += dsp::sanitize (right, numSamples);

    updateHostTempo();
    parameterBridge.update (snapshot);
    inputGain.setTarget (dsp::decibelsToGain (snapshot.get (params::Param::inputGain)));
    outputGain.setTarget (dsp::decibelsToGain (snapshot.get (params::Param::outputGain)));

    for (int i = 0; i < numSamples; ++i)
    {
        const auto gain = inputGain.next();
        left[i] *= gain;
        if (right != nullptr)
            right[i] *= gain;
    }

    if (right != nullptr)
    {
        chain.setSettings (engines::mapToChainSettings (snapshot, hostBpm.load (std::memory_order_relaxed)));
        chain.process (left, right, numSamples);
    }

    for (int i = 0; i < numSamples; ++i)
    {
        const auto gain = outputGain.next();
        left[i] *= gain;
        if (right != nullptr)
            right[i] *= gain;
    }

    // Last line of defence: never hand NaN/Inf to the host.
    replaced += dsp::sanitize (left, numSamples);
    if (right != nullptr)
        replaced += dsp::sanitize (right, numSamples);

    if (replaced > 0)
        nonFiniteSamples.fetch_add (static_cast<std::uint64_t> (replaced), std::memory_order_relaxed);
}

void EchoformAudioProcessor::updateHostTempo() noexcept
{
    // getPosition() is only valid on the audio thread, during processBlock.
    if (auto* hostPlayHead = getPlayHead())
        if (const auto position = hostPlayHead->getPosition())
            if (const auto bpm = position->getBpm())
                if (*bpm >= engines::kMinBpm && *bpm <= engines::kMaxBpm)
                    hostBpm.store (*bpm, std::memory_order_relaxed);
}

double EchoformAudioProcessor::getTailLengthSeconds() const
{
    // Callable from any thread: reads only atomics.
    params::ParameterSnapshot current;
    parameterBridge.update (current);
    return engines::estimateTailSeconds (engines::mapToChainSettings (current, hostBpm.load (std::memory_order_relaxed)));
}

void EchoformAudioProcessor::processBlockBypassed (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ignoreUnused (midi);

    // Host bypass: dry signal, but still a valid stereo image for mono inputs.
    if (buffer.getNumSamples() > 0 && buffer.getNumChannels() > 0)
        prepareStereoChannels (buffer);

    // Keep the engines running on silence so their tails decay instead of
    // resuming when the host re-enables the plug-in.
    juce::ScopedNoDenormals noDenormals;
    chain.processSilence (buffer.getNumSamples());
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
