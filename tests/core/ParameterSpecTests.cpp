#include "params/ParameterSnapshot.h"
#include "params/ParameterSpecs.h"

#include <gtest/gtest.h>

#include <cctype>
#include <limits>
#include <set>
#include <string>

using namespace echoform::params;

TEST (ParameterSpecs, IdsAreUniqueAndHostSafe)
{
    std::set<std::string> seen;
    for (const auto& s : kParamSpecs)
    {
        const std::string id { s.id };
        EXPECT_TRUE (seen.insert (id).second) << "duplicate id " << id;
        EXPECT_FALSE (id.empty());
        EXPECT_LE (id.size(), 32u) << id;

        for (const auto c : id)
            EXPECT_TRUE (std::isalnum (static_cast<unsigned char> (c))) << "id '" << id << "' has '" << c << "'";
    }
    EXPECT_EQ (seen.size(), kNumParams);
}

TEST (ParameterSpecs, TableOrderMatchesEnum)
{
    for (std::size_t i = 0; i < kNumParams; ++i)
        EXPECT_EQ (indexOf (kParamSpecs[i].param), i) << kParamSpecs[i].id;
}

TEST (ParameterSpecs, RangesAndDefaultsAreConsistent)
{
    for (const auto& s : kParamSpecs)
    {
        EXPECT_LT (s.minValue, s.maxValue) << s.id;
        EXPECT_GE (s.defaultValue, s.minValue) << s.id;
        EXPECT_LE (s.defaultValue, s.maxValue) << s.id;

        if (s.kind == Kind::Choice)
        {
            ASSERT_NE (s.choiceNames, nullptr) << s.id;
            EXPECT_FLOAT_EQ (s.maxValue, static_cast<float> (s.numChoices - 1)) << s.id;
            for (std::size_t i = 0; i < s.numChoices; ++i)
                EXPECT_NE (std::string (s.choiceNames[i]), "") << s.id;
        }

        if (s.kind == Kind::Bool)
        {
            EXPECT_FLOAT_EQ (s.minValue, 0.0f);
            EXPECT_FLOAT_EQ (s.maxValue, 1.0f);
        }
    }
}

TEST (ParameterSpecs, ChoiceListsMatchTheirEnums)
{
    EXPECT_EQ (spec (Param::routing).numChoices, 3u);
    EXPECT_EQ (spec (Param::delayMode).numChoices, 4u);
    EXPECT_EQ (spec (Param::reverbMode).numChoices, 5u);
    EXPECT_EQ (spec (Param::delayDivision).numChoices, kTempoDivisions.size());
    EXPECT_EQ (spec (Param::reverbShimmerInterval).numChoices, kShimmerIntervals.size());

    EXPECT_STREQ (kDelayModeNames[static_cast<std::size_t> (DelayMode::PingPong)], "Ping Pong");
    EXPECT_STREQ (kReverbModeNames[static_cast<std::size_t> (ReverbMode::Shimmer)], "Shimmer");
    EXPECT_STREQ (kRoutingNames[static_cast<std::size_t> (Routing::ReverbToDelay)], "Reverb > Delay");
}

TEST (ParameterSpecs, TempoDivisionsAreSortedAndMusicallyCorrect)
{
    for (std::size_t i = 1; i < kTempoDivisions.size(); ++i)
        EXPECT_LT (kTempoDivisions[i - 1].quarterNotes, kTempoDivisions[i].quarterNotes);

    auto find = [] (const std::string& name)
    {
        for (const auto& d : kTempoDivisions)
            if (name == d.name)
                return d.quarterNotes;
        return -1.0;
    };

    EXPECT_DOUBLE_EQ (find ("1/4"), 1.0);
    EXPECT_DOUBLE_EQ (find ("1/8D"), 0.75);
    EXPECT_DOUBLE_EQ (find ("1/8T"), 1.0 / 3.0);
    EXPECT_DOUBLE_EQ (find ("1/1"), 4.0);
    EXPECT_STREQ (kTempoDivisionNames[kDefaultTempoDivision], "1/4");
}

TEST (ParameterSnapshot, StartsAtDefaults)
{
    const ParameterSnapshot snapshot;
    for (const auto& s : kParamSpecs)
        EXPECT_FLOAT_EQ (snapshot.get (s.param), s.defaultValue) << s.id;
}

TEST (ParameterSnapshot, ClampsToRangeAndRejectsNonFinite)
{
    ParameterSnapshot snapshot;

    snapshot.set (Param::delayTime, 1.0e9f);
    EXPECT_FLOAT_EQ (snapshot.get (Param::delayTime), spec (Param::delayTime).maxValue);

    snapshot.set (Param::delayTime, -5.0f);
    EXPECT_FLOAT_EQ (snapshot.get (Param::delayTime), spec (Param::delayTime).minValue);

    snapshot.set (Param::reverbDecay, std::numeric_limits<float>::quiet_NaN());
    EXPECT_FLOAT_EQ (snapshot.get (Param::reverbDecay), spec (Param::reverbDecay).defaultValue);

    snapshot.set (Param::outputGain, std::numeric_limits<float>::infinity());
    EXPECT_FLOAT_EQ (snapshot.get (Param::outputGain), spec (Param::outputGain).defaultValue);
}

TEST (ParameterSnapshot, ChoiceAndBoolAccessors)
{
    ParameterSnapshot snapshot;

    snapshot.set (Param::delayMode, 2.4f);
    EXPECT_EQ (snapshot.getChoiceAs<DelayMode> (Param::delayMode), DelayMode::BucketBrigade);

    snapshot.set (Param::delayMode, 2.6f);
    EXPECT_EQ (snapshot.getChoiceAs<DelayMode> (Param::delayMode), DelayMode::PingPong);

    snapshot.set (Param::reverbFreeze, 1.0f);
    EXPECT_TRUE (snapshot.getBool (Param::reverbFreeze));
    snapshot.set (Param::reverbFreeze, 0.0f);
    EXPECT_FALSE (snapshot.getBool (Param::reverbFreeze));

    EXPECT_EQ (ParameterSnapshot {}.getChoiceAs<Routing> (Param::routing), Routing::DelayToReverb);
}
