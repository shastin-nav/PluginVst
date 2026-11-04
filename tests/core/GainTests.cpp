#include "dsp/Gain.h"

#include <gtest/gtest.h>

#include <cmath>

using echoform::dsp::decibelsToGain;
using echoform::dsp::kUnityToleranceDb;

TEST (Gain, ZeroDecibelsIsExactlyUnity)
{
    EXPECT_EQ (decibelsToGain (0.0f), 1.0f);
    EXPECT_EQ (decibelsToGain (-0.0f), 1.0f);
}

TEST (Gain, NormalisationResidueAroundZeroIsExactlyUnity)
{
    // The value an ARM FMA produces when converting 0 dB back from the
    // normalised value of a -48..+12 dB range (observed on Apple Silicon CI).
    const auto fusedResidue = std::fma (0.8f, 60.0f, -48.0f);
    EXPECT_NE (fusedResidue, 0.0f);
    EXPECT_EQ (decibelsToGain (fusedResidue), 1.0f);
    EXPECT_EQ (decibelsToGain (-fusedResidue), 1.0f);
    EXPECT_EQ (decibelsToGain (kUnityToleranceDb * 0.99f), 1.0f);
}

TEST (Gain, ValuesOutsideToleranceAreConvertedNormally)
{
    EXPECT_NEAR (decibelsToGain (-6.0f), 0.501187f, 1.0e-6f);
    EXPECT_NEAR (decibelsToGain (6.0f), 1.995262f, 1.0e-5f);
    EXPECT_NEAR (decibelsToGain (-48.0f), 0.003981f, 1.0e-6f);
    EXPECT_NEAR (decibelsToGain (12.0f), 3.981072f, 1.0e-5f);
    EXPECT_GT (decibelsToGain (kUnityToleranceDb * 2.0f), 1.0f);
    EXPECT_LT (decibelsToGain (-kUnityToleranceDb * 2.0f), 1.0f);
}

TEST (Gain, IsMonotonic)
{
    float previous = decibelsToGain (-60.0f);
    for (int i = -599; i <= 240; ++i)
    {
        const auto gain = decibelsToGain (static_cast<float> (i) * 0.1f);
        EXPECT_GE (gain, previous) << i;
        previous = gain;
    }
}
