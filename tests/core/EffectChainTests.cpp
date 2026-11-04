#include "AudioAnalysis.h"

#include "engines/EffectChain.h"
#include "engines/ParameterMapping.h"

#include <gtest/gtest.h>

#include <cmath>

using namespace echoform;
using namespace echoform::engines;
using namespace echoform::test;

namespace
{
constexpr double fs = 48000.0;
constexpr float kHalfPi = 1.57079632679489661923f;

struct Stereo
{
    Signal left, right;
};

ChainSettings settingsFor (params::Routing routing, float delayMix, float reverbMix)
{
    ChainSettings c;
    c.routing = routing;
    c.delayMix = delayMix;
    c.reverbMix = reverbMix;
    c.delay.timeSeconds = 0.05f;
    c.delay.feedback = 0.5f;
    c.reverb.decaySeconds = 1.5f;
    return c;
}

Stereo runChain (const ChainSettings& c, const Signal& inL, const Signal& inR, int blockSize = 512)
{
    EffectChain chain;
    chain.prepare (fs);
    chain.setSettings (c);
    Stereo out { inL, inR };
    for (std::size_t done = 0; done < inL.size();)
    {
        const auto n = std::min<std::size_t> (static_cast<std::size_t> (blockSize), inL.size() - done);
        chain.process (out.left.data() + done, out.right.data() + done, static_cast<int> (n));
        done += n;
    }
    return out;
}

Stereo runDelay (const DelaySettings& s, const Stereo& in)
{
    DelayEngine e;
    e.prepare (fs);
    e.setSettings (s);
    Stereo out { Signal (in.left.size()), Signal (in.right.size()) };
    for (std::size_t done = 0; done < in.left.size(); done += EffectChain::kBlockSize)
    {
        const auto n = std::min<std::size_t> (EffectChain::kBlockSize, in.left.size() - done);
        e.process (in.left.data() + done, in.right.data() + done, out.left.data() + done, out.right.data() + done, static_cast<int> (n));
    }
    return out;
}

Stereo runReverb (const ReverbSettings& s, const Stereo& in)
{
    FdnReverb r;
    r.prepare (fs);
    r.setSettings (s);
    Stereo out { Signal (in.left.size()), Signal (in.right.size()) };
    for (std::size_t done = 0; done < in.left.size(); done += EffectChain::kBlockSize)
    {
        const auto n = std::min<std::size_t> (EffectChain::kBlockSize, in.left.size() - done);
        r.process (in.left.data() + done, in.right.data() + done, out.left.data() + done, out.right.data() + done, static_cast<int> (n));
    }
    return out;
}

/// dry * x + wet * w, with the chain's equal-power gains.
Stereo blend (const Stereo& x, const Stereo& w, float mix)
{
    const auto dry = std::sin ((1.0f - mix) * kHalfPi);
    const auto wet = std::sin (mix * kHalfPi);
    Stereo out { x.left, x.right };
    for (std::size_t n = 0; n < x.left.size(); ++n)
    {
        out.left[n] = dry * x.left[n] + wet * w.left[n];
        out.right[n] = dry * x.right[n] + wet * w.right[n];
    }
    return out;
}

void expectClose (const Signal& a, const Signal& b, float tolerance, const char* what)
{
    ASSERT_EQ (a.size(), b.size());
    float worst = 0.0f;
    for (std::size_t n = 0; n < a.size(); ++n)
        worst = std::max (worst, std::abs (a[n] - b[n]));
    EXPECT_LE (worst, tolerance) << what;
}
} // namespace

TEST (EffectChain, ZeroMixOrBypassIsBitTransparentInEveryRouting)
{
    const auto inL = whiteNoise (24000, 1);
    const auto inR = whiteNoise (24000, 2);

    for (const auto routing : { params::Routing::Parallel, params::Routing::DelayToReverb, params::Routing::ReverbToDelay })
    {
        const auto zeroMix = runChain (settingsFor (routing, 0.0f, 0.0f), inL, inR);
        EXPECT_EQ (zeroMix.left, inL);
        EXPECT_EQ (zeroMix.right, inR);

        auto bypassed = settingsFor (routing, 0.8f, 0.8f);
        bypassed.delayEnabled = false;
        bypassed.reverbEnabled = false;
        const auto out = runChain (bypassed, inL, inR);
        EXPECT_EQ (out.left, inL);
        EXPECT_EQ (out.right, inR);
    }
}

TEST (EffectChain, SerialRoutingsEqualTheComposedEngines)
{
    const Stereo x { whiteNoise (48000, 3), whiteNoise (48000, 4) };

    for (const auto routing : { params::Routing::DelayToReverb, params::Routing::ReverbToDelay })
    {
        const auto c = settingsFor (routing, 0.6f, 0.4f);
        const auto chain = runChain (c, x.left, x.right);

        Stereo expected;
        if (routing == params::Routing::DelayToReverb)
        {
            const auto afterDelay = blend (x, runDelay (c.delay, x), c.delayMix);
            expected = blend (afterDelay, runReverb (c.reverb, afterDelay), c.reverbMix);
        }
        else
        {
            const auto afterReverb = blend (x, runReverb (c.reverb, x), c.reverbMix);
            expected = blend (afterReverb, runDelay (c.delay, afterReverb), c.delayMix);
        }

        expectClose (chain.left, expected.left, 1.0e-5f, "left");
        expectClose (chain.right, expected.right, 1.0e-5f, "right");
    }
}

TEST (EffectChain, ParallelRoutingSumsEnginesOverAttenuatedDry)
{
    const Stereo x { whiteNoise (48000, 5), whiteNoise (48000, 6) };
    const auto c = settingsFor (params::Routing::Parallel, 0.3f, 0.7f);
    const auto chain = runChain (c, x.left, x.right);

    const auto d = runDelay (c.delay, x);
    const auto r = runReverb (c.reverb, x);
    const auto dry = std::sin ((1.0f - c.delayMix) * kHalfPi) * std::sin ((1.0f - c.reverbMix) * kHalfPi);
    const auto wetD = std::sin (c.delayMix * kHalfPi);
    const auto wetR = std::sin (c.reverbMix * kHalfPi);

    Signal expectedL (x.left.size()), expectedR (x.right.size());
    for (std::size_t n = 0; n < x.left.size(); ++n)
    {
        expectedL[n] = dry * x.left[n] + wetD * d.left[n] + wetR * r.left[n];
        expectedR[n] = dry * x.right[n] + wetD * d.right[n] + wetR * r.right[n];
    }
    expectClose (chain.left, expectedL, 1.0e-5f, "left");
    expectClose (chain.right, expectedR, 1.0e-5f, "right");
}

TEST (EffectChain, MixFollowsTheEqualPowerLaw)
{
    // Delay only, 100 ms: the output at t = 0 is pure dry, at t = 100 ms pure wet.
    auto c = settingsFor (params::Routing::Parallel, 0.5f, 0.0f);
    c.reverbEnabled = false;
    c.delay.timeSeconds = 0.1f;
    c.delay.feedback = 0.0f;
    c.delay.modDepth = 0.0f;
    const auto in = impulse (9600);
    const auto out = runChain (c, in, in);

    EXPECT_NEAR (out.left[0], std::sqrt (0.5f), 1.0e-6f);
    EXPECT_NEAR (out.left[4800], std::sqrt (0.5f), 2.0e-3f);
}

TEST (EffectChain, OutputDoesNotDependOnBlockSize)
{
    const auto inL = whiteNoise (30000, 7);
    const auto inR = whiteNoise (30000, 8);
    const auto c = settingsFor (params::Routing::DelayToReverb, 0.5f, 0.5f);

    const auto reference = runChain (c, inL, inR, 4096);
    for (const auto block : { 1, 7, 32, 256, 1000 })
    {
        const auto out = runChain (c, inL, inR, block);
        EXPECT_EQ (out.left, reference.left) << block;
        EXPECT_EQ (out.right, reference.right) << block;
    }
}

TEST (EffectChain, BypassToggleIsClickFreeAndMutesTheEngine)
{
    EffectChain chain;
    chain.prepare (fs);
    auto c = settingsFor (params::Routing::DelayToReverb, 0.5f, 0.5f);
    chain.setSettings (c);

    auto tone = sine (static_cast<std::size_t> (fs * 2.0), 330.0, fs, 0.5f);
    Signal left = tone, right = tone;
    const auto half = tone.size() / 2;
    chain.process (left.data(), right.data(), static_cast<int> (half));

    c.delayEnabled = false;
    c.reverbEnabled = false;
    chain.setSettings (c);
    chain.process (left.data() + half, right.data() + half, static_cast<int> (tone.size() - half));

    // The 330 Hz tone itself changes by at most 2*pi*330/fs*0.5 = 0.022 per
    // sample; the wet tail adds a little. A hard switch would step far more.
    EXPECT_LT (maxStep (left, half - 2000, half + 4000), 0.1f);

    // After the 20 ms fade the output is exactly the dry input.
    const auto settled = half + static_cast<std::size_t> (fs * 0.05);
    for (auto n = settled; n < tone.size(); ++n)
        ASSERT_EQ (left[n], tone[n]) << n;
}

TEST (EffectChain, RoutingSwitchFadesThroughSilenceWithoutClicks)
{
    EffectChain chain;
    chain.prepare (fs);
    auto c = settingsFor (params::Routing::DelayToReverb, 0.4f, 0.4f);
    chain.setSettings (c);

    const auto tone = sine (static_cast<std::size_t> (fs * 1.0), 440.0, fs, 0.5f);
    Signal left = tone, right = tone;
    const auto half = tone.size() / 2;
    chain.process (left.data(), right.data(), static_cast<int> (half));
    const auto before = maxStep (left, half - 4800, half);

    c.routing = params::Routing::Parallel;
    chain.setSettings (c);
    chain.process (left.data() + half, right.data() + half, static_cast<int> (tone.size() - half));

    EXPECT_EQ (chain.getActiveRouting(), params::Routing::Parallel);
    EXPECT_LT (maxStep (left, half, half + 4800), 1.5f * before + 0.01f);
    EXPECT_TRUE (allFinite (left) && allFinite (right));
}

TEST (EffectChain, ProcessSilenceLetsTailsDecay)
{
    EffectChain chain;
    chain.prepare (fs);
    auto c = settingsFor (params::Routing::DelayToReverb, 1.0f, 1.0f);
    c.reverb.decaySeconds = 0.5f;
    chain.setSettings (c);

    auto burst = whiteNoise (24000, 9);
    Signal right = burst;
    chain.process (burst.data(), right.data(), 24000);

    chain.processSilence (static_cast<int> (fs * 4.0)); // host bypass: tails keep decaying

    Signal silenceL (4800, 0.0f), silenceR (4800, 0.0f);
    chain.process (silenceL.data(), silenceR.data(), 4800);
    EXPECT_LT (toDb (rms (silenceL)), -90.0) << "no stale tail after a long bypass";
}

TEST (ParameterMapping, ConvertsUnitsAndRouting)
{
    params::ParameterSnapshot p;
    p.set (params::Param::routing, 0.0f);
    p.set (params::Param::delayMix, 40.0f);
    p.set (params::Param::delayTime, 250.0f);
    p.set (params::Param::delayFeedback, 80.0f);
    p.set (params::Param::delayTone, -50.0f);
    p.set (params::Param::delayWidth, 150.0f);
    p.set (params::Param::reverbBypass, 1.0f);
    p.set (params::Param::reverbPreDelay, 20.0f);
    p.set (params::Param::reverbSize, 75.0f);
    p.set (params::Param::reverbFreeze, 1.0f);

    const auto c = mapToChainSettings (p, 120.0);
    EXPECT_EQ (c.routing, params::Routing::Parallel);
    EXPECT_FLOAT_EQ (c.delayMix, 0.4f);
    EXPECT_FLOAT_EQ (c.delay.timeSeconds, 0.25f);
    EXPECT_FLOAT_EQ (c.delay.feedback, 0.8f);
    EXPECT_FLOAT_EQ (c.delay.tone, -0.5f);
    EXPECT_FLOAT_EQ (c.delay.width, 1.5f);
    EXPECT_FALSE (c.reverbEnabled);
    EXPECT_FLOAT_EQ (c.reverb.preDelaySeconds, 0.02f);
    EXPECT_FLOAT_EQ (c.reverb.size, 0.75f);
    EXPECT_TRUE (c.reverb.freeze);

    p.set (params::Param::delaySync, 1.0f);
    p.set (params::Param::delayDivision, static_cast<float> (params::kDefaultTempoDivision)); // 1/4
    EXPECT_FLOAT_EQ (mapToChainSettings (p, 90.0).delay.timeSeconds, static_cast<float> (60.0 / 90.0));
}

TEST (ParameterMapping, TailEstimateFollowsRoutingAndIsCapped)
{
    ChainSettings c;
    c.delay.timeSeconds = 0.5f;
    c.delay.feedback = 0.5f; // 0.5^k = 0.001 -> k = 9.97 repeats
    c.reverb.decaySeconds = 3.0f;
    c.reverb.preDelaySeconds = 0.1f;

    const auto delayTail = 0.5 * (1.0 + std::log (0.001) / std::log (0.5));
    c.routing = params::Routing::DelayToReverb;
    EXPECT_NEAR (estimateTailSeconds (c), delayTail + 3.1, 1.0e-4);
    c.routing = params::Routing::Parallel;
    EXPECT_NEAR (estimateTailSeconds (c), std::max (delayTail, 3.1), 1.0e-4);

    c.reverbEnabled = false;
    EXPECT_NEAR (estimateTailSeconds (c), delayTail, 1.0e-4);
    c.delayMix = 0.0f;
    EXPECT_DOUBLE_EQ (estimateTailSeconds (c), 0.0);

    c.reverbEnabled = true;
    c.reverb.freeze = true;
    EXPECT_DOUBLE_EQ (estimateTailSeconds (c), kMaxReportedTailSeconds);
    c.reverb.freeze = false;
    c.delayMix = 1.0f;
    c.delay.feedback = 1.0f;
    EXPECT_DOUBLE_EQ (estimateTailSeconds (c), kMaxReportedTailSeconds);
}
