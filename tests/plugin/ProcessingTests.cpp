#include "plugin/TestHelpers.h"

#include <gtest/gtest.h>

#include <cmath>
#include <cstring>
#include <limits>

using namespace echoform;
using namespace echoform::test;

namespace
{
void process (EchoformAudioProcessor& p, juce::AudioBuffer<float>& buffer)
{
    juce::MidiBuffer midi;
    p.processBlock (buffer, midi);
}

bool allFinite (const juce::AudioBuffer<float>& buffer)
{
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        for (int i = 0; i < buffer.getNumSamples(); ++i)
            if (! std::isfinite (buffer.getSample (ch, i)))
                return false;
    return true;
}

bool buffersEqual (const juce::AudioBuffer<float>& a, const juce::AudioBuffer<float>& b)
{
    if (a.getNumChannels() != b.getNumChannels() || a.getNumSamples() != b.getNumSamples())
        return false;
    for (int ch = 0; ch < a.getNumChannels(); ++ch)
        for (int i = 0; i < a.getNumSamples(); ++i)
            if (std::memcmp (a.getReadPointer (ch) + i, b.getReadPointer (ch) + i, sizeof (float)) != 0)
                return false;
    return true;
}
} // namespace

TEST (Processing, DefaultSettingsAreBitTransparent)
{
    // P0 has no engines: at 0 dB in/out the signal must pass bit-exact.
    auto processor = makePreparedProcessor();

    for (unsigned block = 0; block < 8; ++block)
    {
        juce::AudioBuffer<float> buffer (2, 512);
        fillWithNoise (buffer, block);
        juce::AudioBuffer<float> reference;
        reference.makeCopyOf (buffer);

        process (*processor, buffer);
        EXPECT_TRUE (buffersEqual (buffer, reference)) << "block " << block;
    }
}

TEST (Processing, MonoInputFeedsBothOutputs)
{
    EchoformAudioProcessor processor;
    juce::AudioProcessor::BusesLayout layout;
    layout.inputBuses.add (juce::AudioChannelSet::mono());
    layout.outputBuses.add (juce::AudioChannelSet::stereo());
    ASSERT_TRUE (processor.setBusesLayout (layout));
    processor.prepareToPlay (44100.0, 256);
    ASSERT_EQ (processor.getTotalNumInputChannels(), 1);
    ASSERT_EQ (processor.getTotalNumOutputChannels(), 2);

    juce::AudioBuffer<float> buffer (2, 256);
    fillWithNoise (buffer, 5);
    for (int i = 0; i < 256; ++i)
        buffer.setSample (1, i, 1234.0f); // garbage in the unfed channel

    juce::AudioBuffer<float> input;
    input.makeCopyOf (buffer);
    process (processor, buffer);

    for (int i = 0; i < 256; ++i)
    {
        EXPECT_EQ (buffer.getSample (0, i), input.getSample (0, i));
        EXPECT_EQ (buffer.getSample (1, i), input.getSample (0, i));
    }
}

TEST (Processing, SupportsOnlyMonoOrStereoIntoStereo)
{
    EchoformAudioProcessor processor;
    const auto supports = [&processor] (const juce::AudioChannelSet& in, const juce::AudioChannelSet& out)
    {
        juce::AudioProcessor::BusesLayout layout;
        layout.inputBuses.add (in);
        layout.outputBuses.add (out);
        return processor.checkBusesLayoutSupported (layout);
    };

    EXPECT_TRUE (supports (juce::AudioChannelSet::stereo(), juce::AudioChannelSet::stereo()));
    EXPECT_TRUE (supports (juce::AudioChannelSet::mono(), juce::AudioChannelSet::stereo()));
    EXPECT_FALSE (supports (juce::AudioChannelSet::mono(), juce::AudioChannelSet::mono()));
    EXPECT_FALSE (supports (juce::AudioChannelSet::stereo(), juce::AudioChannelSet::mono()));
    EXPECT_FALSE (supports (juce::AudioChannelSet::create5point1(), juce::AudioChannelSet::create5point1()));
}

TEST (Processing, OutputGainIsSmoothedThenExact)
{
    constexpr double sampleRate = 48000.0;
    auto processor = makePreparedProcessor (sampleRate, 4096);
    setParameterValue (*processor, "outputGain", -6.0f);

    const auto targetGain = juce::Decibels::decibelsToGain (getParameterValue (*processor, "outputGain"));
    const auto rampSamples = static_cast<int> (sampleRate * EchoformAudioProcessor::kGainSmoothingSeconds);

    juce::AudioBuffer<float> buffer (2, 4096);
    for (int ch = 0; ch < 2; ++ch)
        juce::FloatVectorOperations::fill (buffer.getWritePointer (ch), 1.0f, 4096);
    process (*processor, buffer);

    // During the ramp the gain moves monotonically in small steps (no zipper).
    const auto maxStep = (1.0f - targetGain) / static_cast<float> (rampSamples) * 1.01f;
    for (int i = 1; i < rampSamples; ++i)
    {
        const auto previous = buffer.getSample (0, i - 1);
        const auto current = buffer.getSample (0, i);
        EXPECT_LE (current, previous);
        EXPECT_LE (previous - current, maxStep) << "sample " << i;
    }

    // After the ramp the gain is exact.
    for (int i = rampSamples; i < 4096; ++i)
    {
        EXPECT_FLOAT_EQ (buffer.getSample (0, i), targetGain);
        EXPECT_FLOAT_EQ (buffer.getSample (1, i), targetGain);
    }
}

TEST (Processing, HandlesArbitraryBlockSizes)
{
    // Hosts may deliver blocks of any size, including larger than announced.
    auto processor = makePreparedProcessor (48000.0, 256);

    for (const auto size : { 1, 2, 3, 7, 31, 64, 127, 256, 257, 1000, 4096, 8192 })
    {
        juce::AudioBuffer<float> buffer (2, size);
        fillWithNoise (buffer, static_cast<unsigned> (size));
        juce::AudioBuffer<float> reference;
        reference.makeCopyOf (buffer);

        process (*processor, buffer);
        EXPECT_TRUE (buffersEqual (buffer, reference)) << "block size " << size;
    }

    juce::AudioBuffer<float> empty (2, 0);
    process (*processor, empty); // must not crash
}

TEST (Processing, SampleRateChangesReinitialiseSmoothing)
{
    EchoformAudioProcessor processor;

    for (const auto rate : { 22050.0, 44100.0, 48000.0, 88200.0, 96000.0, 176400.0, 192000.0, 44100.0 })
    {
        processor.setRateAndBufferSizeDetails (rate, 512);
        processor.prepareToPlay (rate, 512);

        // Settled state after prepare: transparent.
        juce::AudioBuffer<float> buffer (2, 512);
        fillWithNoise (buffer, 3);
        juce::AudioBuffer<float> reference;
        reference.makeCopyOf (buffer);
        process (processor, buffer);
        EXPECT_TRUE (buffersEqual (buffer, reference)) << rate;

        // Ramp length scales with the sample rate (constant 20 ms).
        setParameterValue (processor, "outputGain", -12.0f);
        const auto rampSamples = static_cast<int> (rate * EchoformAudioProcessor::kGainSmoothingSeconds);
        juce::AudioBuffer<float> ones (2, rampSamples + 64);
        for (int ch = 0; ch < 2; ++ch)
            juce::FloatVectorOperations::fill (ones.getWritePointer (ch), 1.0f, ones.getNumSamples());
        process (processor, ones);

        const auto target = juce::Decibels::decibelsToGain (getParameterValue (processor, "outputGain"));
        EXPECT_GT (ones.getSample (0, rampSamples - 2), target) << rate;
        EXPECT_FLOAT_EQ (ones.getSample (0, rampSamples - 1), target) << rate;

        setParameterValue (processor, "outputGain", 0.0f);
    }
}

TEST (Processing, NonFiniteInputNeverReachesTheOutput)
{
    auto processor = makePreparedProcessor();
    juce::AudioBuffer<float> buffer (2, 64);
    fillWithNoise (buffer, 11);

    buffer.setSample (0, 3, std::numeric_limits<float>::quiet_NaN());
    buffer.setSample (1, 10, std::numeric_limits<float>::infinity());
    buffer.setSample (0, 20, -std::numeric_limits<float>::infinity());

    process (*processor, buffer);

    EXPECT_TRUE (allFinite (buffer));
    EXPECT_EQ (processor->getNonFiniteSampleCount(), 3u);

    // Subsequent clean blocks are unaffected.
    juce::AudioBuffer<float> clean (2, 64);
    fillWithNoise (clean, 12);
    juce::AudioBuffer<float> reference;
    reference.makeCopyOf (clean);
    process (*processor, clean);
    EXPECT_TRUE (buffersEqual (clean, reference));
}

TEST (Processing, BypassPassesDrySignalAndKeepsMonoImage)
{
    EchoformAudioProcessor processor;
    juce::AudioProcessor::BusesLayout layout;
    layout.inputBuses.add (juce::AudioChannelSet::mono());
    layout.outputBuses.add (juce::AudioChannelSet::stereo());
    ASSERT_TRUE (processor.setBusesLayout (layout));
    processor.prepareToPlay (48000.0, 128);
    setParameterValue (processor, "outputGain", -20.0f);

    juce::AudioBuffer<float> buffer (2, 128);
    fillWithNoise (buffer, 21);
    juce::AudioBuffer<float> input;
    input.makeCopyOf (buffer);

    juce::MidiBuffer midi;
    processor.processBlockBypassed (buffer, midi);

    for (int i = 0; i < 128; ++i)
    {
        EXPECT_EQ (buffer.getSample (0, i), input.getSample (0, i));
        EXPECT_EQ (buffer.getSample (1, i), input.getSample (0, i));
    }
}

TEST (Processing, InitialisationIsDeterministic)
{
    auto a = makePreparedProcessor (44100.0, 300);
    auto b = makePreparedProcessor (44100.0, 300);
    setParameterValue (*a, "inputGain", 4.5f);
    setParameterValue (*b, "inputGain", 4.5f);

    for (unsigned block = 0; block < 5; ++block)
    {
        juce::AudioBuffer<float> bufA (2, 300);
        fillWithNoise (bufA, 100 + block);
        juce::AudioBuffer<float> bufB;
        bufB.makeCopyOf (bufA);

        process (*a, bufA);
        process (*b, bufB);
        EXPECT_TRUE (buffersEqual (bufA, bufB)) << "block " << block;
    }
}
