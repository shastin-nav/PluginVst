#pragma once

#include "dsp/LinearSmoother.h"
#include "params/ParameterSnapshot.h"
#include "plugin/ParameterBridge.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <atomic>
#include <cstdint>

namespace echoform
{

/// ECHOFORM audio processor.
///
/// Milestone P0: owns the complete parameter set, state serialisation, bus
/// handling and the real-time-safe signal path skeleton (input gain -> [engines,
/// not yet implemented] -> output gain -> NaN/Inf guard).
class EchoformAudioProcessor final : public juce::AudioProcessor
{
public:
    EchoformAudioProcessor();
    ~EchoformAudioProcessor() override = default;

    // ---- Lifecycle --------------------------------------------------------------------------
    void prepareToPlay (double sampleRate, int maximumExpectedSamplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    // ---- Audio ------------------------------------------------------------------------------
    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override;
    void processBlockBypassed (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override;
    using juce::AudioProcessor::processBlock;
    using juce::AudioProcessor::processBlockBypassed;

    // ---- Editor -----------------------------------------------------------------------------
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    // ---- Description ------------------------------------------------------------------------
    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    // ---- Programs (presets are handled by ECHOFORM's own preset system) ---------------------
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    // ---- State ------------------------------------------------------------------------------
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // ---- Accessors --------------------------------------------------------------------------
    juce::AudioProcessorValueTreeState& getValueTreeState() noexcept { return state; }

    /// Total number of NaN/Inf samples removed from the output (diagnostics; lock-free).
    std::uint64_t getNonFiniteSampleCount() const noexcept { return nonFiniteSamples.load (std::memory_order_relaxed); }

    static constexpr double kGainSmoothingSeconds = 0.02;

private:
    /// Makes the buffer a valid stereo signal: duplicates a mono input to both
    /// outputs and clears any output channel without a matching input.
    void prepareStereoChannels (juce::AudioBuffer<float>& buffer) const noexcept;

    juce::AudioProcessorValueTreeState state;
    ParameterBridge parameterBridge;
    params::ParameterSnapshot snapshot;

    dsp::LinearSmoother inputGain;
    dsp::LinearSmoother outputGain;

    std::atomic<std::uint64_t> nonFiniteSamples { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EchoformAudioProcessor)
};

} // namespace echoform
