#include "AudioAnalysis.h"

#include "engines/DelayEngine.h"
#include "engines/ParameterMapping.h"

#include <gtest/gtest.h>

#include <cmath>

using namespace echoform;
using namespace echoform::engines;
using namespace echoform::test;

namespace
{
DelaySettings dry (float timeSeconds)
{
    DelaySettings s;
    s.timeSeconds = timeSeconds;
    s.feedback = 0.0f;
    s.tone = 0.0f;
    s.modDepth = 0.0f;
    s.width = 1.0f;
    return s;
}

struct Stereo
{
    Signal left, right;
};

Stereo render (DelayEngine& engine, const Signal& inL, const Signal& inR, int blockSize = 256)
{
    Stereo out { Signal (inL.size()), Signal (inR.size()) };
    for (std::size_t done = 0; done < inL.size();)
    {
        const auto n = std::min<std::size_t> (static_cast<std::size_t> (blockSize), inL.size() - done);
        engine.process (inL.data() + done, inR.data() + done, out.left.data() + done, out.right.data() + done, static_cast<int> (n));
        done += n;
    }
    return out;
}

DelayEngine makeEngine (double fs, const DelaySettings& s)
{
    DelayEngine e;
    e.prepare (fs);
    e.setSettings (s);
    return e;
}

double windowRms (const Signal& s, std::size_t centre, std::size_t halfWidth)
{
    return rms (s, centre > halfWidth ? centre - halfWidth : 0, centre + halfWidth);
}
} // namespace

TEST (DelayEngine, EchoArrivesAtExactDelayAtEverySampleRate)
{
    for (const auto fs : { 44100.0, 48000.0, 88200.0, 96000.0, 192000.0 })
    {
        auto engine = makeEngine (fs, dry (0.25f));
        const auto in = impulse (static_cast<std::size_t> (fs * 0.4));
        const auto out = render (engine, in, in);

        const auto expected = static_cast<std::size_t> (std::lround (0.25 * fs));
        EXPECT_EQ (peakIndex (out.left), expected) << fs;
        EXPECT_EQ (peakIndex (out.right), expected) << fs;
        EXPECT_GT (out.left[expected], 0.99f) << fs;
        EXPECT_EQ (peakAbs (out.left, 0, expected - 2), 0.0f) << "output before the echo at " << fs;
    }
}

TEST (DelayEngine, FractionalDelayIsInterpolatedAtTheRightPosition)
{
    // Phase delay of a 1 kHz sine, measured relative to an integer-delay
    // reference: the loop's 15 Hz high-pass shifts both identically, so only
    // the fractional part of the delay remains.
    constexpr double fs = 48000.0;
    constexpr double hz = 1000.0;
    const auto omega = 2.0 * kPi * hz / fs;

    auto phase = [&] (double delaySamples)
    {
        auto engine = makeEngine (fs, dry (static_cast<float> (delaySamples / fs)));
        const auto in = sine (static_cast<std::size_t> (fs * 0.3), hz, fs);
        const auto out = render (engine, in, in);
        double sinSum = 0.0, cosSum = 0.0;
        for (auto n = static_cast<std::size_t> (fs * 0.1); n < out.left.size(); ++n)
        {
            sinSum += static_cast<double> (out.left[n]) * std::sin (omega * static_cast<double> (n));
            cosSum += static_cast<double> (out.left[n]) * std::cos (omega * static_cast<double> (n));
        }
        return std::atan2 (cosSum, sinSum); // = -omega * delay + constant
    };

    const auto reference = phase (489.0);
    for (const auto fraction : { 0.25, 0.5, 0.6, 0.875 })
        EXPECT_NEAR ((reference - phase (489.0 + fraction)) / omega, fraction, 0.01) << fraction;
}

TEST (DelayEngine, TempoSyncResolvesNoteDivisions)
{
    auto indexOf = [] (const char* name)
    {
        for (std::size_t i = 0; i < params::kTempoDivisions.size(); ++i)
            if (std::string (params::kTempoDivisions[i].name) == name)
                return static_cast<int> (i);
        return -1;
    };

    EXPECT_DOUBLE_EQ (resolveDelaySeconds (true, 999.0f, indexOf ("1/4"), 120.0), 0.5);
    EXPECT_DOUBLE_EQ (resolveDelaySeconds (true, 999.0f, indexOf ("1/8D"), 100.0), 0.45);
    EXPECT_DOUBLE_EQ (resolveDelaySeconds (true, 999.0f, indexOf ("1/8T"), 90.0), 1.0 / 3.0 * 60.0 / 90.0);
    EXPECT_DOUBLE_EQ (resolveDelaySeconds (true, 999.0f, indexOf ("1/1"), 60.0), 4.0);
    EXPECT_DOUBLE_EQ (resolveDelaySeconds (false, 375.0f, indexOf ("1/1"), 60.0), 0.375);

    // Invalid host tempo falls back to 120 BPM.
    EXPECT_DOUBLE_EQ (resolveDelaySeconds (true, 0.0f, indexOf ("1/4"), 0.0), 0.5);
    EXPECT_DOUBLE_EQ (resolveDelaySeconds (true, 0.0f, indexOf ("1/4"), std::nan ("")), 0.5);

    // The engine clamps to its buffer: 1/1D at 20 BPM is 18 s.
    auto s = dry (static_cast<float> (resolveDelaySeconds (true, 0.0f, indexOf ("1/1D"), 20.0)));
    auto engine = makeEngine (48000.0, s);
    Signal silence (64, 0.0f);
    render (engine, silence, silence);
    EXPECT_FLOAT_EQ (engine.getCurrentDelaySamples(), static_cast<float> (DelayEngine::kMaxDelaySeconds * 48000.0));
}

TEST (DelayEngine, FeedbackProducesGeometricallyDecayingEchoes)
{
    constexpr double fs = 48000.0;
    auto s = dry (0.1f);
    s.feedback = 0.5f;
    auto engine = makeEngine (fs, s);
    const auto in = impulse (static_cast<std::size_t> (fs * 1.0));
    const auto out = render (engine, in, in);

    for (int k = 1; k <= 8; ++k)
    {
        const auto at = static_cast<std::size_t> (4800 * k);
        const auto expected = std::pow (0.5, k - 1);
        EXPECT_NEAR (out.left[at], expected, 0.02 * expected + 1.0e-4) << "echo " << k;
        EXPECT_EQ (peakIndex (out.left, at - 100, at + 100), at) << "echo " << k;
    }
}

TEST (DelayEngine, TailDecaysBelowMinus60dB)
{
    constexpr double fs = 48000.0;
    auto s = dry (0.05f);
    s.feedback = 0.7f;
    auto engine = makeEngine (fs, s);
    const auto in = whiteNoise (static_cast<std::size_t> (fs * 4.0), 5);
    Signal burst = in;
    std::fill (burst.begin() + static_cast<long> (fs * 0.1), burst.end(), 0.0f);
    const auto out = render (engine, burst, burst);

    // 0.7^k < 0.001 after ~20 repeats (1 s at 50 ms).
    const auto early = rms (out.left, static_cast<std::size_t> (fs * 0.05), static_cast<std::size_t> (fs * 0.15));
    const auto late = rms (out.left, static_cast<std::size_t> (fs * 1.5), static_cast<std::size_t> (fs * 2.0));
    EXPECT_LT (toDb (late) - toDb (early), -60.0);
}

TEST (DelayEngine, FullFeedbackStaysBoundedAndFiniteInEveryMode)
{
    constexpr double fs = 48000.0;
    const auto loud = whiteNoise (static_cast<std::size_t> (fs * 20.0), 9, 1.0f);

    for (const auto mode : { params::DelayMode::Digital, params::DelayMode::PingPong })
    {
        for (const auto tone : { -1.0f, 0.0f, 1.0f })
        {
            DelaySettings s;
            s.mode = mode;
            s.timeSeconds = 0.013f;
            s.feedback = 1.0f;
            s.tone = tone;
            s.modDepth = 1.0f;
            s.modRateHz = 10.0f;
            s.width = 2.0f;
            auto engine = makeEngine (fs, s);
            const auto out = render (engine, loud, loud);

            EXPECT_TRUE (allFinite (out.left) && allFinite (out.right));
            EXPECT_LE (peakAbs (out.left), DelayEngine::kOutputBound) << "tone " << tone;
            EXPECT_LE (peakAbs (out.right), DelayEngine::kOutputBound) << "tone " << tone;

            // Stable means no growth: the last 5 s are not louder than seconds 5-10.
            const auto middle = toDb (rms (out.left, static_cast<std::size_t> (fs * 5), static_cast<std::size_t> (fs * 10)));
            const auto end = toDb (rms (out.left, static_cast<std::size_t> (fs * 15)));
            EXPECT_LT (end - middle, 1.0) << "tone " << tone;
        }
    }
}

TEST (DelayEngine, GrossOverloadIsContainedBySaturation)
{
    constexpr double fs = 48000.0;
    auto s = dry (0.01f);
    s.feedback = 1.0f;
    auto engine = makeEngine (fs, s);
    const auto overload = whiteNoise (static_cast<std::size_t> (fs * 5.0), 13, 50.0f); // +34 dBFS
    const auto out = render (engine, overload, overload);
    EXPECT_TRUE (allFinite (out.left));
    EXPECT_LE (peakAbs (out.left), DelayEngine::kOutputBound);
}

TEST (DelayEngine, PingPongAlternatesBetweenChannels)
{
    constexpr double fs = 48000.0;
    auto s = dry (0.1f);
    s.mode = params::DelayMode::PingPong;
    s.feedback = 0.6f;
    auto engine = makeEngine (fs, s);
    const auto in = impulse (static_cast<std::size_t> (fs * 0.7));
    const auto out = render (engine, in, in);

    for (int k = 1; k <= 5; ++k)
    {
        const auto at = static_cast<std::size_t> (4800 * k);
        const auto& loud = (k % 2 == 1) ? out.left : out.right;
        const auto& quiet = (k % 2 == 1) ? out.right : out.left;
        const auto separation = toDb (windowRms (loud, at, 8)) - toDb (windowRms (quiet, at, 8));
        EXPECT_GT (separation, 40.0) << "echo " << k;
    }
}

TEST (DelayEngine, WidthZeroIsMonoAndWidthChangesSide)
{
    constexpr double fs = 48000.0;
    auto s = dry (0.02f);
    s.modDepth = 1.0f; // L/R modulation is in quadrature, so channels differ
    s.modRateHz = 3.0f;
    s.feedback = 0.4f;
    const auto inL = whiteNoise (48000, 1);
    const auto inR = whiteNoise (48000, 2);

    s.width = 0.0f;
    auto mono = makeEngine (fs, s);
    const auto monoOut = render (mono, inL, inR);
    EXPECT_EQ (monoOut.left, monoOut.right);

    s.width = 1.0f;
    auto normal = makeEngine (fs, s);
    const auto normalOut = render (normal, inL, inR);
    s.width = 2.0f;
    auto wide = makeEngine (fs, s);
    const auto wideOut = render (wide, inL, inR);

    const auto from = std::size_t { 2000 };
    EXPECT_LT (correlation (wideOut.left, wideOut.right, from, 48000), correlation (normalOut.left, normalOut.right, from, 48000));
}

TEST (DelayEngine, DarkToneLowersHighFrequenciesOnEveryRepeat)
{
    constexpr double fs = 48000.0;
    auto s = dry (0.05f);
    s.feedback = 0.9f;
    s.tone = -0.8f;
    auto engine = makeEngine (fs, s);
    const auto out = render (engine, impulse (24000), impulse (24000));

    // Energy of the first difference relative to the energy: a measure of
    // high-frequency content. It must fall from one repeat to the next.
    double previousRatio = 1.0e9;
    for (int k = 1; k <= 6; ++k)
    {
        const auto at = static_cast<std::size_t> (2400 * k);
        double energy = 0.0, diffEnergy = 0.0;
        for (auto n = at - 200; n < at + 1000; ++n)
        {
            energy += static_cast<double> (out.left[n]) * out.left[n];
            const auto d = static_cast<double> (out.left[n] - out.left[n - 1]);
            diffEnergy += d * d;
        }
        const auto ratio = diffEnergy / energy;
        EXPECT_LT (ratio, previousRatio) << "repeat " << k;
        previousRatio = ratio;
    }
}

TEST (DelayEngine, BrightToneCutsLowFrequencies)
{
    constexpr double fs = 48000.0;
    auto s = dry (0.05f);
    s.tone = 1.0f;
    auto bright = makeEngine (fs, s);
    auto neutral = makeEngine (fs, dry (0.05f));

    const auto low = sine (48000, 80.0, fs);
    const auto brightLow = render (bright, low, low);
    const auto neutralLow = render (neutral, low, low);
    EXPECT_LT (toDb (rms (brightLow.left, 24000)) - toDb (rms (neutralLow.left, 24000)), -10.0);
}

TEST (DelayEngine, TimeChangesCrossfadeWithoutClicks)
{
    constexpr double fs = 48000.0;
    auto engine = makeEngine (fs, dry (0.3f));
    const auto tone = sine (static_cast<std::size_t> (fs * 1.0), 440.0, fs, 0.8f);

    Stereo out { Signal (tone.size()), Signal (tone.size()) };
    const auto change = static_cast<std::size_t> (fs * 0.5);
    engine.process (tone.data(), tone.data(), out.left.data(), out.right.data(), static_cast<int> (change));
    engine.setSettings (dry (0.12f));
    engine.process (tone.data() + change, tone.data() + change, out.left.data() + change, out.right.data() + change,
                    static_cast<int> (tone.size() - change));

    // A 440 Hz sine at 0.8 changes by at most 2*pi*440/fs*0.8 = 0.046 per sample.
    // An instantaneous jump between the two taps could step by up to 1.6.
    EXPECT_LT (maxStep (out.left, change - 10, change + 4000), 0.08f);
    EXPECT_FLOAT_EQ (engine.getCurrentDelaySamples(), static_cast<float> (0.12 * fs));
}

TEST (DelayEngine, ModulationBendsPitchWithinTheDesignedBound)
{
    constexpr double fs = 48000.0;
    auto s = dry (0.02f);
    s.modDepth = 1.0f;
    s.modRateHz = 2.0f;
    auto engine = makeEngine (fs, s);
    const auto tone = sine (static_cast<std::size_t> (fs * 2.0), 1000.0, fs);
    const auto out = render (engine, tone, tone);

    // Measure the period between rising zero crossings after the echo starts.
    double minPeriod = 1.0e9, maxPeriod = 0.0, lastCrossing = -1.0;
    for (std::size_t n = 2000; n < out.left.size(); ++n)
    {
        if (out.left[n - 1] < 0.0f && out.left[n] >= 0.0f)
        {
            const auto crossing = static_cast<double> (n - 1)
                                  + static_cast<double> (-out.left[n - 1] / (out.left[n] - out.left[n - 1]));
            if (lastCrossing > 0.0)
            {
                minPeriod = std::min (minPeriod, crossing - lastCrossing);
                maxPeriod = std::max (maxPeriod, crossing - lastCrossing);
            }
            lastCrossing = crossing;
        }
    }

    // Peak pitch deviation = 2*pi*rate*excursion = 2*pi*2*0.003 = 3.8 %.
    const auto nominal = fs / 1000.0;
    const auto bound = 2.0 * kPi * 2.0 * DelayEngine::kMaxModulationSeconds;
    EXPECT_GT ((maxPeriod - minPeriod) / nominal, bound);              // it does modulate
    EXPECT_LT ((maxPeriod - minPeriod) / nominal, 2.0 * bound * 1.1);  // but within +/- bound
}

TEST (DelayEngine, ResetSilencesAndProcessingIsDeterministic)
{
    constexpr double fs = 44100.0;
    DelaySettings s;
    s.feedback = 0.8f;
    s.modDepth = 0.5f;
    const auto noise = whiteNoise (44100, 21);

    auto a = makeEngine (fs, s);
    auto b = makeEngine (fs, s);
    const auto outA = render (a, noise, noise, 64);
    const auto outB = render (b, noise, noise, 1000);
    EXPECT_EQ (outA.left, outB.left) << "output must not depend on block size";

    a.reset();
    a.setSettings (s);
    const Signal silence (44100, 0.0f);
    const auto afterReset = render (a, silence, silence);
    EXPECT_EQ (peakAbs (afterReset.left), 0.0f);
}
