#include "plugin/TestHelpers.h"

#include "params/ParameterSpecs.h"
#include "plugin/ParameterBridge.h"

#include <gtest/gtest.h>

using namespace echoform;
using namespace echoform::params;
using namespace echoform::test;

TEST (ParameterLayout, ExposesExactlyTheSpecTableInOrder)
{
    EchoformAudioProcessor processor;
    const auto& hostParams = processor.getParameters();

    ASSERT_EQ (static_cast<std::size_t> (hostParams.size()), kNumParams);

    for (std::size_t i = 0; i < kNumParams; ++i)
    {
        auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (hostParams[static_cast<int> (i)]);
        ASSERT_NE (ranged, nullptr);
        EXPECT_EQ (ranged->getParameterID(), juce::String (kParamSpecs[i].id)) << "host index " << i;
        EXPECT_EQ (ranged->getName (64), juce::String (kParamSpecs[i].name));
        EXPECT_TRUE (ranged->isAutomatable());
    }
}

TEST (ParameterLayout, RangesAndDefaultsMatchSpecs)
{
    EchoformAudioProcessor processor;

    for (const auto& s : kParamSpecs)
    {
        auto& p = parameter (processor, s.id);
        const auto& range = p.getNormalisableRange();

        EXPECT_FLOAT_EQ (range.start, s.minValue) << s.id;
        EXPECT_FLOAT_EQ (range.end, s.maxValue) << s.id;
        EXPECT_NEAR (p.convertFrom0to1 (p.getDefaultValue()), s.defaultValue, 1.0e-3f) << s.id;
        EXPECT_NEAR (getParameterValue (processor, s.id), s.defaultValue, 1.0e-3f) << s.id;

        if (s.kind == Kind::Choice)
        {
            auto* choice = dynamic_cast<juce::AudioParameterChoice*> (&p);
            ASSERT_NE (choice, nullptr) << s.id;
            EXPECT_EQ (static_cast<std::size_t> (choice->choices.size()), s.numChoices) << s.id;
        }
        if (s.kind == Kind::Bool)
        {
            EXPECT_NE (dynamic_cast<juce::AudioParameterBool*> (&p), nullptr) << s.id;
        }
        if (s.kind == Kind::Float)
        {
            EXPECT_NE (dynamic_cast<juce::AudioParameterFloat*> (&p), nullptr) << s.id;
        }
    }
}

TEST (ParameterLayout, SkewedRangesPutCentreValueAtMidpoint)
{
    EchoformAudioProcessor processor;

    for (const auto& s : kParamSpecs)
    {
        if (s.skewCentre <= 0.0f)
            continue;
        auto& p = parameter (processor, s.id);
        EXPECT_NEAR (p.convertTo0to1 (s.skewCentre), 0.5f, 1.0e-3f) << s.id;
    }
}

TEST (ParameterLayout, BridgeReflectsEveryParameter)
{
    EchoformAudioProcessor processor;
    ParameterBridge bridge { processor.getValueTreeState() };

    for (const auto& s : kParamSpecs)
    {
        auto& p = parameter (processor, s.id);
        const auto newNormalised = p.getValue() < 0.5f ? 0.9f : 0.1f;
        p.setValueNotifyingHost (newNormalised);

        ParameterSnapshot snapshot;
        bridge.update (snapshot);

        const auto expected = p.convertFrom0to1 (p.getValue());
        EXPECT_NEAR (snapshot.get (s.param), expected, 1.0e-4f) << s.id;
        EXPECT_GT (std::abs (snapshot.get (s.param) - s.defaultValue), 0.0f) << s.id << " did not change";
    }
}

TEST (ParameterLayout, ValueTextIsHumanReadableAndParsesBack)
{
    EchoformAudioProcessor processor;

    auto& time = parameter (processor, "delayTime");
    EXPECT_EQ (time.getText (time.convertTo0to1 (375.0f), 32), "375 ms");
    EXPECT_EQ (time.getText (time.convertTo0to1 (1200.0f), 32), "1.20 s");
    EXPECT_NEAR (time.convertFrom0to1 (time.getValueForText ("1.2 s")), 1200.0f, 0.5f);
    EXPECT_NEAR (time.convertFrom0to1 (time.getValueForText ("250 ms")), 250.0f, 0.5f);

    auto& gain = parameter (processor, "outputGain");
    EXPECT_EQ (gain.getText (gain.convertTo0to1 (3.0f), 32), "+3.0 dB");
    EXPECT_EQ (gain.getText (gain.convertTo0to1 (-6.0f), 32), "-6.0 dB");

    auto& decay = parameter (processor, "reverbDecay");
    EXPECT_EQ (decay.getText (decay.convertTo0to1 (2.5f), 32), "2.50 s");

    auto& mix = parameter (processor, "delayMix");
    EXPECT_EQ (mix.getText (mix.convertTo0to1 (35.0f), 32), "35 %");

    auto& mode = parameter (processor, "delayMode");
    EXPECT_EQ (mode.getText (mode.convertTo0to1 (3.0f), 32), "Ping Pong");
}
