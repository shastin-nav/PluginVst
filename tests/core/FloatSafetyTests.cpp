#include "dsp/FloatSafety.h"

#include <gtest/gtest.h>

#include <array>
#include <limits>

using echoform::dsp::isFinite;
using echoform::dsp::sanitize;

TEST (FloatSafety, ClassifiesFiniteValues)
{
    EXPECT_TRUE (isFinite (0.0f));
    EXPECT_TRUE (isFinite (-0.0f));
    EXPECT_TRUE (isFinite (1.0f));
    EXPECT_TRUE (isFinite (-123456.0f));
    EXPECT_TRUE (isFinite (std::numeric_limits<float>::max()));
    EXPECT_TRUE (isFinite (std::numeric_limits<float>::lowest()));
    EXPECT_TRUE (isFinite (std::numeric_limits<float>::denorm_min()));
}

TEST (FloatSafety, ClassifiesNonFiniteValues)
{
    EXPECT_FALSE (isFinite (std::numeric_limits<float>::infinity()));
    EXPECT_FALSE (isFinite (-std::numeric_limits<float>::infinity()));
    EXPECT_FALSE (isFinite (std::numeric_limits<float>::quiet_NaN()));
    EXPECT_FALSE (isFinite (std::numeric_limits<float>::signaling_NaN()));
}

TEST (FloatSafety, SanitizeReplacesOnlyNonFiniteSamples)
{
    constexpr auto inf = std::numeric_limits<float>::infinity();
    constexpr auto nan = std::numeric_limits<float>::quiet_NaN();

    std::array<float, 7> samples { 0.5f, nan, -0.25f, inf, -inf, 1.0e-40f, 0.0f };
    const auto replaced = sanitize (samples.data(), static_cast<int> (samples.size()));

    EXPECT_EQ (replaced, 3);
    EXPECT_FLOAT_EQ (samples[0], 0.5f);
    EXPECT_FLOAT_EQ (samples[1], 0.0f);
    EXPECT_FLOAT_EQ (samples[2], -0.25f);
    EXPECT_FLOAT_EQ (samples[3], 0.0f);
    EXPECT_FLOAT_EQ (samples[4], 0.0f);
    EXPECT_TRUE (isFinite (samples[5]));
    EXPECT_FLOAT_EQ (samples[6], 0.0f);
}

TEST (FloatSafety, SanitizeHandlesEmptyInput)
{
    EXPECT_EQ (sanitize (nullptr, 0), 0);
}
