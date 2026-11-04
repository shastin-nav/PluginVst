#include "AudioAnalysis.h"

#include "dsp/Allpass.h"
#include "dsp/DelayLine.h"
#include "dsp/Filters.h"
#include "dsp/Hadamard.h"
#include "dsp/QuadratureOscillator.h"
#include "dsp/Saturation.h"

#include <gtest/gtest.h>

#include <array>
#include <cmath>

using namespace echoform;
using namespace echoform::test;

// ---- DelayLine ------------------------------------------------------------------------------

TEST (DelayLine, IntegerReadsReturnExactPastSamples)
{
    dsp::DelayLine line;
    line.prepare (100);
    for (int n = 0; n < 1000; ++n)
    {
        if (n >= 37)
        {
            EXPECT_EQ (line.readInteger (37), static_cast<float> (n - 37));
            EXPECT_EQ (line.readInteger (1), static_cast<float> (n - 1));
        }
        line.write (static_cast<float> (n));
    }
}

TEST (DelayLine, InterpolatedReadsAreExactAtIntegerPositions)
{
    dsp::DelayLine line;
    line.prepare (64);
    const auto noise = whiteNoise (200, 3);
    for (const auto x : noise)
        line.write (x);

    for (int d = 2; d < 60; ++d)
    {
        EXPECT_EQ (line.readLinear (static_cast<float> (d)), line.readInteger (d));
        EXPECT_EQ (line.readCubic (static_cast<float> (d)), line.readInteger (d));
    }
}

TEST (DelayLine, CubicReproducesQuadraticsAndLinearReproducesRamps)
{
    // Catmull-Rom (Keys a = -1/2) is exact for polynomials up to degree 2.
    dsp::DelayLine quadratic;
    dsp::DelayLine ramp;
    quadratic.prepare (64);
    ramp.prepare (64);
    constexpr int written = 100;
    for (int n = 0; n < written; ++n)
    {
        quadratic.write (0.001f * static_cast<float> (n * n));
        ramp.write (static_cast<float> (n));
    }

    for (const auto d : { 2.25f, 7.5f, 13.125f, 40.875f })
    {
        const auto t = static_cast<double> (written) - static_cast<double> (d); // time index read
        EXPECT_NEAR (quadratic.readCubic (d), 0.001 * t * t, 1.0e-3) << d;
        EXPECT_NEAR (ramp.readLinear (d), t, 1.0e-4) << d;
        EXPECT_NEAR (ramp.readCubic (d), t, 1.0e-4) << d;
    }
}

TEST (DelayLine, WrapsAroundAndResetClears)
{
    dsp::DelayLine line;
    line.prepare (10);
    const auto capacity = static_cast<int> (line.getCapacity());
    for (int n = 0; n < capacity * 5 + 3; ++n)
        line.write (static_cast<float> (n));
    const auto last = capacity * 5 + 2;
    EXPECT_EQ (line.readInteger (1), static_cast<float> (last));
    EXPECT_EQ (line.readInteger (10), static_cast<float> (last - 9));

    line.reset();
    for (int d = 1; d <= 10; ++d)
        EXPECT_EQ (line.readInteger (d), 0.0f);
}

// ---- Allpass --------------------------------------------------------------------------------

TEST (SchroederAllpass, ImpulseResponseHasUnitEnergy)
{
    // An all-pass filter preserves energy: sum h[n]^2 = 1.
    dsp::SchroederAllpass ap;
    ap.prepare (37);
    ap.setGain (0.7f);

    double energy = 0.0;
    for (int n = 0; n < 37 * 200; ++n)
    {
        const auto y = static_cast<double> (ap.process (n == 0 ? 1.0f : 0.0f));
        energy += y * y;
    }
    EXPECT_NEAR (energy, 1.0, 1.0e-4);
}

// ---- Hadamard -------------------------------------------------------------------------------

TEST (Hadamard, IsOrthogonalAndSelfInverse)
{
    const auto input = whiteNoise (16, 11);
    std::array<float, 16> x {};
    std::copy (input.begin(), input.end(), x.begin());

    auto y = x;
    dsp::hadamardInPlace<16> (y.data());

    double inEnergy = 0.0, outEnergy = 0.0;
    for (std::size_t i = 0; i < 16; ++i)
    {
        inEnergy += static_cast<double> (x[i]) * x[i];
        outEnergy += static_cast<double> (y[i]) * y[i];
    }
    EXPECT_NEAR (outEnergy, inEnergy, 1.0e-6 * inEnergy);

    dsp::hadamardInPlace<16> (y.data());
    for (std::size_t i = 0; i < 16; ++i)
        EXPECT_NEAR (y[i], x[i], 1.0e-6f);

    // Every output depends on every input (maximal mixing).
    std::array<float, 16> unit {};
    unit[5] = 1.0f;
    dsp::hadamardInPlace<16> (unit.data());
    for (const auto v : unit)
        EXPECT_FLOAT_EQ (std::abs (v), 0.25f);
}

// ---- Oscillator -----------------------------------------------------------------------------

TEST (QuadratureOscillator, TracksTrueSineWithStableAmplitude)
{
    constexpr double fs = 48000.0;
    constexpr double hz = 3.7;
    dsp::QuadratureOscillator osc;
    osc.setFrequency (hz, fs);
    osc.reset (0.0);

    constexpr int steps = 48000 * 60; // one minute
    double worstPhaseError = 0.0;
    for (int n = 0; n < steps; ++n)
    {
        if (n % 4801 == 0)
        {
            const auto expected = std::sin (2.0 * kPi * hz * static_cast<double> (n) / fs);
            worstPhaseError = std::max (worstPhaseError, std::abs (static_cast<double> (osc.sine()) - expected));
        }
        osc.advance();
    }
    const auto amplitude = std::hypot (static_cast<double> (osc.sine()), static_cast<double> (osc.cosine()));
    EXPECT_NEAR (amplitude, 1.0, 1.0e-5);
    EXPECT_LT (worstPhaseError, 2.0e-3);
}

// ---- Saturation -----------------------------------------------------------------------------

TEST (SoftClip, TransparentBelowThresholdBoundedMonotonicAndSmooth)
{
    for (const auto x : { 0.0f, 0.3f, -0.7f, 1.0f, -1.0f })
        EXPECT_EQ (dsp::softClip (x), x);

    float previous = dsp::softClip (-100.0f);
    for (int i = -10000; i <= 10000; ++i)
    {
        const auto x = static_cast<float> (i) * 0.01f;
        const auto y = dsp::softClip (x);
        EXPECT_LE (std::abs (y), 2.0f);
        EXPECT_GE (y, previous);
        EXPECT_FLOAT_EQ (dsp::softClip (-x), -y);
        previous = y;
    }

    // Slope is continuous at the threshold (= 1 on both sides).
    const auto h = 1.0e-3f;
    EXPECT_NEAR ((dsp::softClip (1.0f + h) - dsp::softClip (1.0f)) / h, 1.0f, 2.0e-3f);
}

// ---- Filters --------------------------------------------------------------------------------

TEST (OnePoleLowpass, DcAndNyquistGainsMatchDesign)
{
    constexpr float a = 0.6f;
    dsp::OnePoleLowpass dc;
    dsp::OnePoleLowpass nyquist;
    dc.setCoefficient (a);
    nyquist.setCoefficient (a);

    float yDc = 0.0f, yNy = 0.0f;
    for (int n = 0; n < 2000; ++n)
    {
        yDc = dc.process (1.0f);
        yNy = nyquist.process (n % 2 == 0 ? 1.0f : -1.0f);
    }
    EXPECT_NEAR (yDc, 1.0f, 1.0e-6f);
    EXPECT_NEAR (std::abs (yNy), (1.0f - a) / (1.0f + a), 1.0e-5f);
}

TEST (DcBlocker, RemovesDcAndPassesAudio)
{
    dsp::DcBlocker blocker;
    blocker.setCutoff (10.0, 48000.0);
    const auto tone = sine (96000, 1000.0, 48000.0);

    float last = 0.0f;
    for (int n = 0; n < 96000; ++n)
        last = blocker.process (0.5f);
    EXPECT_LT (std::abs (last), 1.0e-3f);

    blocker.reset();
    Signal out (tone.size());
    for (std::size_t n = 0; n < tone.size(); ++n)
        out[n] = blocker.process (tone[n]);
    EXPECT_NEAR (rms (out, 48000), rms (tone, 48000), 1.0e-3);
}
