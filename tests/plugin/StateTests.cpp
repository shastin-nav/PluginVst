#include "plugin/TestHelpers.h"

#include "params/ParameterSpecs.h"
#include "plugin/StateSerializer.h"

#include <gtest/gtest.h>

#include <cmath>
#include <random>
#include <vector>

using namespace echoform;
using namespace echoform::params;
using namespace echoform::test;

namespace
{
juce::MemoryBlock stateOf (EchoformAudioProcessor& p)
{
    juce::MemoryBlock block;
    p.getStateInformation (block);
    return block;
}

juce::MemoryBlock blobFromXml (const juce::String& xmlText)
{
    const auto xml = juce::parseXML (xmlText);
    juce::MemoryBlock block;
    if (xml != nullptr)
        juce::AudioProcessor::copyXmlToBinary (*xml, block);
    return block;
}

void randomiseAll (EchoformAudioProcessor& p, unsigned seed)
{
    std::mt19937 rng { seed };
    std::uniform_real_distribution<float> dist { 0.0f, 1.0f };
    for (auto* param : p.getParameters())
        param->setValueNotifyingHost (dist (rng));
}

/// Parameter values in their natural units. (Compared instead of raw
/// normalised floats: e.g. a JUCE bool parameter set to 0.8 and one set to 1.0
/// both mean "on" and are stored identically.)
std::vector<float> parameterValues (EchoformAudioProcessor& p)
{
    std::vector<float> values;
    for (auto* param : p.getParameters())
    {
        auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (param);
        values.push_back (ranged->convertFrom0to1 (ranged->getValue()));
    }
    return values;
}
} // namespace

TEST (State, RoundTripRestoresEveryParameter)
{
    EchoformAudioProcessor source;
    randomiseAll (source, 1234);
    const auto expected = parameterValues (source);
    const auto blob = stateOf (source);

    EchoformAudioProcessor destination;
    destination.setStateInformation (blob.getData(), static_cast<int> (blob.getSize()));
    const auto actual = parameterValues (destination);

    ASSERT_EQ (actual.size(), expected.size());
    for (std::size_t i = 0; i < expected.size(); ++i)
        EXPECT_NEAR (actual[i], expected[i], 1.0e-4f * (1.0f + std::abs (expected[i]))) << kParamSpecs[i].id;
}

TEST (State, BlobCarriesSchemaAndPluginVersion)
{
    EchoformAudioProcessor processor;
    const auto blob = stateOf (processor);
    const auto xml = juce::AudioProcessor::getXmlFromBinary (blob.getData(), static_cast<int> (blob.getSize()));

    ASSERT_NE (xml, nullptr);
    EXPECT_TRUE (xml->hasTagName (state::kStateType));
    EXPECT_EQ (xml->getIntAttribute (state::kSchemaVersionProperty), state::kSchemaVersion);
    EXPECT_EQ (xml->getStringAttribute (state::kPluginVersionProperty), juce::String (JucePlugin_VersionString));
}

TEST (State, MissingParametersLoadAsDefaultsNotAsCurrentValues)
{
    // A partial state (e.g. written by an older version that lacked some
    // parameters) must not leave stale live values behind.
    EchoformAudioProcessor processor;
    setParameterValue (processor, "delayFeedback", 90.0f);
    setParameterValue (processor, "reverbDecay", 12.0f);

    const auto blob = blobFromXml (R"(<ECHOFORM_STATE schemaVersion="1">
                                        <PARAM id="delayTime" value="500"/>
                                      </ECHOFORM_STATE>)");
    processor.setStateInformation (blob.getData(), static_cast<int> (blob.getSize()));

    EXPECT_NEAR (getParameterValue (processor, "delayTime"), 500.0f, 0.01f);
    EXPECT_NEAR (getParameterValue (processor, "delayFeedback"), spec (Param::delayFeedback).defaultValue, 0.01f);
    EXPECT_NEAR (getParameterValue (processor, "reverbDecay"), spec (Param::reverbDecay).defaultValue, 0.01f);
}

TEST (State, OutOfRangeAndNonFiniteValuesAreSanitised)
{
    EchoformAudioProcessor processor;
    const auto blob = blobFromXml (R"(<ECHOFORM_STATE schemaVersion="1">
                                        <PARAM id="delayTime" value="999999"/>
                                        <PARAM id="outputGain" value="-1000"/>
                                        <PARAM id="reverbDecay" value="nan"/>
                                        <PARAM id="delayMode" value="2.7"/>
                                        <PARAM id="unknownFutureParam" value="1"/>
                                      </ECHOFORM_STATE>)");
    processor.setStateInformation (blob.getData(), static_cast<int> (blob.getSize()));

    EXPECT_NEAR (getParameterValue (processor, "delayTime"), spec (Param::delayTime).maxValue, 0.01f);
    EXPECT_NEAR (getParameterValue (processor, "outputGain"), spec (Param::outputGain).minValue, 0.01f);
    EXPECT_NEAR (getParameterValue (processor, "reverbDecay"), spec (Param::reverbDecay).defaultValue, 0.01f);
    EXPECT_NEAR (getParameterValue (processor, "delayMode"), 3.0f, 0.01f);
}

TEST (State, InvalidBlobsLeaveStateUntouched)
{
    EchoformAudioProcessor processor;
    setParameterValue (processor, "delayMix", 77.0f);
    const auto before = parameterValues (processor);

    const auto tryLoad = [&processor] (const juce::MemoryBlock& blob)
    { processor.setStateInformation (blob.getData(), static_cast<int> (blob.getSize())); };

    // Random bytes.
    juce::MemoryBlock garbage (4096);
    juce::Random random { 42 };
    random.fillBitsRandomly (garbage.getData(), garbage.getSize());
    tryLoad (garbage);

    // Valid XML from another product.
    tryLoad (blobFromXml (R"(<SOMEONE_ELSE><PARAM id="delayMix" value="5"/></SOMEONE_ELSE>)"));

    // Empty / null input.
    processor.setStateInformation (nullptr, 0);
    processor.setStateInformation (garbage.getData(), 3);

    EXPECT_EQ (parameterValues (processor), before);
}

TEST (State, LoadingIsDeterministic)
{
    EchoformAudioProcessor source;
    randomiseAll (source, 99);
    const auto blob = stateOf (source);

    EchoformAudioProcessor a;
    EchoformAudioProcessor b;
    randomiseAll (b, 7); // different live values before loading

    a.setStateInformation (blob.getData(), static_cast<int> (blob.getSize()));
    b.setStateInformation (blob.getData(), static_cast<int> (blob.getSize()));

    EXPECT_EQ (parameterValues (a), parameterValues (b));
    EXPECT_EQ (stateOf (a), stateOf (b));
}
