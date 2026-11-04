#include "dsp/LinearSmoother.h"

#include <gtest/gtest.h>

#include <cmath>

using echoform::dsp::LinearSmoother;

TEST (LinearSmoother, ResetSnapsToInitialValueWithoutRamp)
{
    LinearSmoother s;
    s.reset (48000.0, 0.02, 0.75f);

    EXPECT_FALSE (s.isSmoothing());
    EXPECT_FLOAT_EQ (s.getCurrent(), 0.75f);
    EXPECT_FLOAT_EQ (s.next(), 0.75f);
    EXPECT_EQ (s.getRampLengthSamples(), 960);
}

TEST (LinearSmoother, ReachesTargetExactlyAfterRampLength)
{
    LinearSmoother s;
    s.reset (48000.0, 0.02, 0.0f);
    s.setTarget (1.0f);

    float value = 0.0f;
    for (int i = 0; i < 959; ++i)
        value = s.next();

    EXPECT_TRUE (s.isSmoothing());
    EXPECT_LT (value, 1.0f);

    value = s.next();
    EXPECT_FALSE (s.isSmoothing());
    EXPECT_EQ (value, 1.0f) << "ramp must land exactly on the target";
}

TEST (LinearSmoother, RampIsMonotonicWithBoundedStep)
{
    LinearSmoother s;
    s.reset (44100.0, 0.05, 1.0f);
    s.setTarget (0.0f);

    const auto rampLength = s.getRampLengthSamples();
    const auto maxStep = 1.0f / static_cast<float> (rampLength) * 1.001f;

    float previous = 1.0f;
    while (s.isSmoothing())
    {
        const auto value = s.next();
        EXPECT_LE (value, previous);
        EXPECT_LE (previous - value, maxStep);
        EXPECT_GE (value, 0.0f);
        previous = value;
    }
    EXPECT_EQ (previous, 0.0f);
}

TEST (LinearSmoother, RetargetingSameValueDoesNotRestartRamp)
{
    LinearSmoother s;
    s.reset (1000.0, 0.1, 0.0f); // 100-sample ramp
    s.setTarget (1.0f);

    for (int i = 0; i < 50; ++i)
        s.next();

    s.setTarget (1.0f);
    for (int i = 0; i < 50; ++i)
        s.next();

    EXPECT_FALSE (s.isSmoothing());
    EXPECT_EQ (s.getCurrent(), 1.0f);
}

TEST (LinearSmoother, RetargetMidRampStartsFromCurrentValue)
{
    LinearSmoother s;
    s.reset (1000.0, 0.1, 0.0f);
    s.setTarget (1.0f);
    for (int i = 0; i < 50; ++i)
        s.next();

    const auto midValue = s.getCurrent();
    s.setTarget (0.0f);
    const auto afterRetarget = s.next();

    EXPECT_NEAR (afterRetarget, midValue - midValue / 100.0f, 1.0e-6f);
}

TEST (LinearSmoother, ZeroRampLengthSnapsImmediately)
{
    LinearSmoother s;
    s.reset (48000.0, 0.0, 0.0f);
    s.setTarget (0.3f);

    EXPECT_FALSE (s.isSmoothing());
    EXPECT_FLOAT_EQ (s.next(), 0.3f);
}

TEST (LinearSmoother, SkipMatchesRepeatedNext)
{
    LinearSmoother a;
    LinearSmoother b;
    a.reset (48000.0, 0.01, -1.0f);
    b.reset (48000.0, 0.01, -1.0f);
    a.setTarget (2.0f);
    b.setTarget (2.0f);

    for (int i = 0; i < 123; ++i)
        a.next();
    b.skip (123);
    EXPECT_NEAR (a.getCurrent(), b.getCurrent(), 1.0e-5f);

    b.skip (100000);
    EXPECT_FALSE (b.isSmoothing());
    EXPECT_EQ (b.getCurrent(), 2.0f);
}

TEST (LinearSmoother, RampDurationScalesWithSampleRate)
{
    for (const auto rate : { 22050.0, 44100.0, 48000.0, 96000.0, 192000.0 })
    {
        LinearSmoother s;
        s.reset (rate, 0.02, 0.0f);
        EXPECT_EQ (s.getRampLengthSamples(), static_cast<int> (std::floor (rate * 0.02))) << rate;
    }
}
