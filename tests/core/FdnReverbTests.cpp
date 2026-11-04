#include "AudioAnalysis.h"

#include "engines/FdnReverb.h"

#include <gtest/gtest.h>

#include <cmath>

using namespace echoform;
using namespace echoform::engines;
using namespace echoform::test;

namespace
{
struct Stereo
{
    Signal left, right;
};

ReverbSettings measurementSettings (float decaySeconds)
{
    ReverbSettings s;
    s.decaySeconds = decaySeconds;
    s.damping = 0.0f;
    s.modDepth = 0.0f;
    s.preDelaySeconds = 0.0f;
    return s;
}

Stereo render (FdnReverb& reverb, const Signal& inL, const Signal& inR, int blockSize = 512)
{
    Stereo out { Signal (inL.size()), Signal (inR.size()) };
    for (std::size_t done = 0; done < inL.size();)
    {
        const auto n = std::min<std::size_t> (static_cast<std::size_t> (blockSize), inL.size() - done);
        reverb.process (inL.data() + done, inR.data() + done, out.left.data() + done, out.right.data() + done, static_cast<int> (n));
        done += n;
    }
    return out;
}

Stereo impulseResponse (double fs, const ReverbSettings& s, double seconds)
{
    FdnReverb reverb;
    reverb.prepare (fs);
    reverb.setSettings (s);
    const auto in = impulse (static_cast<std::size_t> (fs * seconds));
    return render (reverb, in, in);
}

double measuredRt60 (double fs, const ReverbSettings& s)
{
    const auto ir = impulseResponse (fs, s, static_cast<double> (s.decaySeconds) * 1.2 + 0.5);
    return estimateRt60 (ir.left, ir.right, fs);
}
} // namespace

TEST (FdnReverb, MeasuredRt60MatchesSetting)
{
    for (const auto decay : { 0.4f, 1.0f, 2.5f, 6.0f })
    {
        const auto rt60 = measuredRt60 (48000.0, measurementSettings (decay));
        EXPECT_NEAR (rt60, decay, 0.10 * decay) << "decay setting " << decay << " s";
    }
}

TEST (FdnReverb, Rt60IsIndependentOfSampleRateAndSize)
{
    for (const auto fs : { 44100.0, 96000.0 })
    {
        const auto rt60 = measuredRt60 (fs, measurementSettings (2.0f));
        EXPECT_NEAR (rt60, 2.0, 0.2) << fs;
    }

    for (const auto size : { 0.0f, 1.0f })
    {
        auto s = measurementSettings (2.0f);
        s.size = size;
        EXPECT_NEAR (measuredRt60 (48000.0, s), 2.0, 0.2) << "size " << size;
    }
}

TEST (FdnReverb, ModulationDoesNotChangeTheDecayTime)
{
    auto s = measurementSettings (2.0f);
    s.modDepth = 1.0f;
    s.modRateHz = 1.0f;
    EXPECT_NEAR (measuredRt60 (48000.0, s), 2.0, 0.3);
}

TEST (FdnReverb, DampingShortensHighFrequencyDecayOnly)
{
    constexpr double fs = 48000.0;
    auto s = measurementSettings (3.0f);
    s.damping = 0.8f; // HF T60 ratio at Nyquist: 1 - 0.9 * 0.8 = 0.28
    const auto ir = impulseResponse (fs, s, 4.5);

    auto lowL = Biquad::lowpass (300.0, fs).process (ir.left);
    auto lowR = Biquad::lowpass (300.0, fs).process (ir.right);
    auto highL = Biquad::highpass (8000.0, fs).process (ir.left);
    auto highR = Biquad::highpass (8000.0, fs).process (ir.right);

    const auto lowRt60 = estimateRt60 (lowL, lowR, fs);
    const auto highRt60 = estimateRt60 (highL, highR, fs);
    EXPECT_NEAR (lowRt60, 3.0, 0.45) << "low-frequency decay follows the decay setting";
    EXPECT_GT (highRt60, 0.0);
    EXPECT_LT (highRt60, 0.6 * lowRt60) << "high frequencies decay faster";
}

TEST (FdnReverb, FreezeSustainsWithoutGrowingAndUnfreezeDecays)
{
    constexpr double fs = 48000.0;
    FdnReverb reverb;
    reverb.prepare (fs);
    auto s = measurementSettings (2.0f);
    s.modDepth = 0.5f;
    reverb.setSettings (s);

    // Excite with a noise burst, then freeze.
    const auto burst = whiteNoise (static_cast<std::size_t> (fs * 0.5), 31, 0.5f);
    render (reverb, burst, burst);
    s.freeze = true;
    reverb.setSettings (s);

    // While frozen, new input must not enter the tank.
    const auto loud = whiteNoise (static_cast<std::size_t> (fs * 10.0), 32, 1.0f);
    const auto frozen = render (reverb, loud, loud);

    std::vector<double> levels;
    for (int second = 1; second < 10; ++second)
        levels.push_back (toDb (rms (frozen.left, static_cast<std::size_t> (fs * second), static_cast<std::size_t> (fs * (second + 1)))));

    const auto [lo, hi] = std::minmax_element (levels.begin(), levels.end());
    EXPECT_LT (*hi - *lo, 1.0) << "frozen level must stay constant";
    EXPECT_LE (levels.back(), levels.front() + 0.05) << "frozen tail must not grow";
    EXPECT_GT (levels.back(), -40.0) << "frozen tail must still be there";

    s.freeze = false;
    reverb.setSettings (s);
    const Signal silence (static_cast<std::size_t> (fs * 6.0), 0.0f);
    const auto released = render (reverb, silence, silence);
    const auto end = toDb (rms (released.left, static_cast<std::size_t> (fs * 5.0)));
    EXPECT_LT (end - levels.back(), -100.0) << "after unfreezing, the tail decays (3 x RT60 = -180 dB)";
}

TEST (FdnReverb, PreDelayDelaysTheOnset)
{
    constexpr double fs = 48000.0;
    auto s = measurementSettings (1.0f);
    s.preDelaySeconds = 0.1f;
    s.size = 0.0f;
    const auto ir = impulseResponse (fs, s, 0.5);

    const auto preDelay = static_cast<std::size_t> (0.1 * fs);
    EXPECT_EQ (peakAbs (ir.left, 0, preDelay), 0.0f);
    EXPECT_EQ (peakAbs (ir.right, 0, preDelay), 0.0f);

    // First tank output after pre-delay + shortest line at size 0 (17.3 ms * 0.4).
    const auto shortest = static_cast<std::size_t> (FdnReverb::baseLineSeconds()[0] * FdnReverb::kMinSizeScale * fs);
    EXPECT_EQ (peakAbs (ir.left, 0, preDelay + shortest - 2), 0.0f);
    EXPECT_GT (peakAbs (ir.left, preDelay, preDelay + shortest + 200), 0.0f);
}

TEST (FdnReverb, WidthControlsStereoDecorrelation)
{
    constexpr double fs = 48000.0;
    const auto noise = whiteNoise (static_cast<std::size_t> (fs * 2.0), 41);
    const auto from = static_cast<std::size_t> (fs * 0.2);
    const auto to = static_cast<std::size_t> (fs * 2.0);

    auto s = measurementSettings (2.0f);
    s.width = 0.0f;
    FdnReverb mono;
    mono.prepare (fs);
    mono.setSettings (s);
    const auto monoOut = render (mono, noise, noise);
    EXPECT_EQ (monoOut.left, monoOut.right);

    s.width = 1.0f;
    FdnReverb stereo;
    stereo.prepare (fs);
    stereo.setSettings (s);
    const auto stereoOut = render (stereo, noise, noise); // identical (mono) input
    EXPECT_LT (std::abs (correlation (stereoOut.left, stereoOut.right, from, to)), 0.3)
        << "a mono source must produce a decorrelated stereo tail";
}

TEST (FdnReverb, OutputIsBoundedAndFiniteAtExtremeSettings)
{
    constexpr double fs = 48000.0;
    ReverbSettings s;
    s.decaySeconds = 30.0f;
    s.size = 1.0f;
    s.damping = 0.0f;
    s.diffusion = 1.0f;
    s.modDepth = 1.0f;
    s.modRateHz = 5.0f;
    s.width = 2.0f;

    FdnReverb reverb;
    reverb.prepare (fs);
    reverb.setSettings (s);
    const auto noise = whiteNoise (static_cast<std::size_t> (fs * 20.0), 51, 1.0f);
    const auto out = render (reverb, noise, noise);
    EXPECT_TRUE (allFinite (out.left) && allFinite (out.right));

    // Unit impulse: no sample exceeds full scale.
    const auto ir = impulseResponse (fs, measurementSettings (2.5f), 1.0);
    EXPECT_LT (peakAbs (ir.left), 1.0f);
    EXPECT_LT (peakAbs (ir.right), 1.0f);

    // Steady-state level at the longest decay stays within a sane range of the input.
    const auto level = toDb (rms (out.left, static_cast<std::size_t> (fs * 15.0))) - toDb (rms (noise));
    EXPECT_LT (level, 24.0);
}

TEST (FdnReverb, ParameterChangesAreSmooth)
{
    constexpr double fs = 48000.0;
    FdnReverb reverb;
    reverb.prepare (fs);
    auto s = measurementSettings (2.0f);
    reverb.setSettings (s);
    const auto tone = sine (static_cast<std::size_t> (fs * 2.0), 220.0, fs, 0.5f);

    Stereo out { Signal (tone.size()), Signal (tone.size()) };
    const auto half = tone.size() / 2;
    reverb.process (tone.data(), tone.data(), out.left.data(), out.right.data(), static_cast<int> (half));
    const auto stepBefore = maxStep (out.left, half - 4800, half);

    s.size = 1.0f;
    s.decaySeconds = 8.0f;
    s.preDelaySeconds = 0.2f;
    s.damping = 1.0f;
    s.width = 0.0f;
    reverb.setSettings (s);
    reverb.process (tone.data() + half, tone.data() + half, out.left.data() + half, out.right.data() + half,
                    static_cast<int> (tone.size() - half));

    EXPECT_TRUE (allFinite (out.left));
    EXPECT_LT (maxStep (out.left, half, half + 4800), 3.0f * stepBefore + 0.01f)
        << "abrupt parameter changes must not produce clicks";
}

TEST (FdnReverb, ResetSilencesAndOutputIsDeterministic)
{
    constexpr double fs = 44100.0;
    ReverbSettings s;
    const auto noise = whiteNoise (22050, 61);

    FdnReverb a, b;
    a.prepare (fs);
    b.prepare (fs);
    a.setSettings (s);
    b.setSettings (s);
    EXPECT_EQ (render (a, noise, noise, 32).left, render (b, noise, noise, 32).left);

    a.reset();
    a.setSettings (s);
    const Signal silence (22050, 0.0f);
    EXPECT_EQ (peakAbs (render (a, silence, silence).left), 0.0f);
}
