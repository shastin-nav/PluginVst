#include "engines/FdnReverb.h"

#include "dsp/Denormals.h"
#include "dsp/FloatSafety.h"
#include "dsp/Hadamard.h"

#include <algorithm>
#include <cmath>

namespace echoform::engines
{
namespace
{
    // Mutually incommensurate lengths spanning ~17-91 ms (total ~0.78 s at
    // size 1.0), giving dense, non-periodic modal structure.
    constexpr std::array<double, FdnReverb::kNumLines> kBaseLineSeconds {
        0.0173, 0.0199, 0.0221, 0.0247, 0.0279, 0.0313, 0.0346, 0.0389,
        0.0433, 0.0481, 0.0537, 0.0593, 0.0661, 0.0737, 0.0823, 0.0911
    };

    // Input diffuser lengths (seconds); slightly different per channel.
    constexpr std::array<std::array<double, FdnReverb::kNumDiffusers>, 2> kDiffuserSeconds { {
        { 0.00477, 0.00359, 0.01273, 0.00931 },
        { 0.00461, 0.00381, 0.01227, 0.00979 },
    } };

    constexpr double kDcBlockHz = 10.0;
    constexpr double kGoldenAngle = 2.39996322972865332;

    /// +1/-1 Walsh sign patterns. Distinct non-zero masks give mutually
    /// orthogonal vectors, so the two outputs (and the two inputs) are
    /// decorrelated combinations of the lines.
    constexpr float walshSign (std::size_t index, std::size_t mask) noexcept
    {
        std::size_t bits = index & mask;
        bool odd = false;
        while (bits != 0)
        {
            odd = ! odd;
            bits &= bits - 1;
        }
        return odd ? -1.0f : 1.0f;
    }

    constexpr std::size_t kOutputMaskL = 0x5;
    constexpr std::size_t kOutputMaskR = 0x3;
    constexpr std::size_t kInputMaskL = 0x6;
    constexpr std::size_t kInputMaskR = 0x9;

    /// Per-line output normalisation: 1/sqrt(N).
    constexpr float kLineNormalisation = 0.25f;
    static_assert (FdnReverb::kNumLines == 16, "kLineNormalisation assumes 16 lines");
} // namespace

const std::array<double, FdnReverb::kNumLines>& FdnReverb::baseLineSeconds() noexcept { return kBaseLineSeconds; }

void FdnReverb::prepare (double newSampleRate)
{
    sampleRate = newSampleRate > 0.0 ? newSampleRate : 44100.0;

    for (auto& dc : dcBlockers)
        dc.setCutoff (kDcBlockHz, sampleRate);

    const auto preDelayCapacity = static_cast<int> (std::ceil (kMaxPreDelaySeconds * sampleRate)) + 6;
    for (auto& line : preDelay)
        line.prepare (preDelayCapacity);

    for (std::size_t ch = 0; ch < 2; ++ch)
    {
        for (std::size_t k = 0; k < kNumDiffusers; ++k)
        {
            const auto length = std::max (1, static_cast<int> (std::lround (kDiffuserSeconds[ch][k] * sampleRate)));
            diffusers[ch][k].prepare (length);
            diffusers[ch][k].setDelay (length);
        }
    }

    maxModulationSamples = static_cast<float> (kMaxModulationSeconds * sampleRate);
    double longest = 0.0;
    for (std::size_t i = 0; i < kNumLines; ++i)
    {
        baseLengthSamples[i] = static_cast<float> (kBaseLineSeconds[i] * sampleRate);
        longest = std::max (longest, kBaseLineSeconds[i]);

        const auto phase = kGoldenAngle * static_cast<double> (i);
        modulationCos[i] = static_cast<float> (std::cos (phase));
        modulationSin[i] = static_cast<float> (std::sin (phase));
    }

    maxReadDelay = static_cast<float> (longest * kMaxSizeScale * sampleRate) + maxModulationSamples;
    for (auto& line : lines)
        line.prepare (static_cast<int> (std::ceil (maxReadDelay)) + 4);

    sizeScale.reset (sampleRate, kSizeSmoothingSeconds, 1.0f);
    preDelaySamples.reset (sampleRate, kParameterSmoothingSeconds, 1.0f);
    modulationDepth.reset (sampleRate, kParameterSmoothingSeconds, 0.0f);
    width.reset (sampleRate, kParameterSmoothingSeconds, 1.0f);
    decaySeconds.reset (sampleRate, kParameterSmoothingSeconds, 2.5f);
    dampingRatio.reset (sampleRate, kParameterSmoothingSeconds, 1.0f);
    diffusionGain.reset (sampleRate, kParameterSmoothingSeconds, 0.0f);
    freezeAmount.reset (sampleRate, kFreezeRampSeconds, 0.0f);

    reset();
}

void FdnReverb::reset() noexcept
{
    for (auto& dc : dcBlockers)
        dc.reset();
    for (auto& line : preDelay)
        line.reset();
    for (auto& channel : diffusers)
        for (auto& ap : channel)
            ap.reset();
    for (auto& line : lines)
        line.reset();
    for (auto& d : damping)
        d.reset();

    lfo.reset();
    snapOnNextSettings = true;
}

void FdnReverb::setSettings (const ReverbSettings& s) noexcept
{
    if (sampleRate <= 0.0)
        return;

    const auto size = std::clamp (s.size, 0.0f, 1.0f);
    sizeScale.setTarget (static_cast<float> (kMinSizeScale + (kMaxSizeScale - kMinSizeScale) * static_cast<double> (size)));

    const auto preDelayTime = std::clamp (static_cast<double> (s.preDelaySeconds), 0.0, kMaxPreDelaySeconds);
    // The pre-delay writes before it reads, so read(d) is a delay of d - 1 samples.
    preDelaySamples.setTarget (static_cast<float> (preDelayTime * sampleRate) + 1.0f);

    modulationDepth.setTarget (std::clamp (s.modDepth, 0.0f, 1.0f) * maxModulationSamples);
    width.setTarget (std::clamp (s.width, 0.0f, 2.0f));
    decaySeconds.setTarget (std::clamp (s.decaySeconds, 0.05f, 60.0f));

    const auto dampingAmount = static_cast<double> (std::clamp (s.damping, 0.0f, 1.0f));
    dampingRatio.setTarget (static_cast<float> (1.0 - (1.0 - kMinDampingRatio) * dampingAmount));

    diffusionGain.setTarget (static_cast<float> (kMaxDiffusionGain * static_cast<double> (std::clamp (s.diffusion, 0.0f, 1.0f))));
    freezeAmount.setTarget (s.freeze ? 1.0f : 0.0f);
    lfo.setFrequency (static_cast<double> (std::max (0.0f, s.modRateHz)), sampleRate);

    if (snapOnNextSettings)
    {
        for (auto* smoother : { &sizeScale, &preDelaySamples, &modulationDepth, &width, &decaySeconds, &dampingRatio,
                                &diffusionGain, &freezeAmount })
            smoother->snapTo (smoother->getTarget());
        snapOnNextSettings = false;
    }
}

void FdnReverb::updateCoefficients() noexcept
{
    const auto t60 = static_cast<double> (decaySeconds.getCurrent());
    const auto alpha = static_cast<double> (dampingRatio.getCurrent());
    const auto scale = static_cast<double> (sizeScale.getCurrent());
    const auto freeze = currentFreeze;

    const auto dampingExponent = 1.0 / alpha - 1.0;

    for (std::size_t i = 0; i < kNumLines; ++i)
    {
        const auto lengthSeconds = static_cast<double> (baseLengthSamples[i]) * scale / sampleRate;
        const auto lowGain = std::pow (10.0, -3.0 * lengthSeconds / t60);     // DC gain for T60
        const auto nyquistRatio = std::pow (lowGain, dampingExponent);        // g_hf / g_dc
        const auto coefficient = (1.0 - nyquistRatio) / (1.0 + nyquistRatio); // exact at DC and Nyquist

        loopGains[i] = static_cast<float> (lowGain) + freeze * (kFreezeLoopGain - static_cast<float> (lowGain));
        damping[i].setCoefficient (static_cast<float> (coefficient) * (1.0f - freeze));
    }

    const auto g = diffusionGain.getCurrent();
    for (auto& channel : diffusers)
        for (auto& ap : channel)
            ap.setGain (g);

    tankInputGain = 1.0f - freeze;
}

void FdnReverb::processSample (float inL, float inR, float& outL, float& outR) noexcept
{
    // Input stage: DC block, pre-delay, diffusion.
    const auto preDelayNow = preDelaySamples.next();
    std::array<float, 2> input { dcBlockers[0].process (inL), dcBlockers[1].process (inR) };
    for (std::size_t ch = 0; ch < 2; ++ch)
    {
        preDelay[ch].write (input[ch]);
        auto x = preDelay[ch].readLinear (preDelayNow);
        for (auto& ap : diffusers[ch])
            x = ap.process (x);
        input[ch] = x * tankInputGain;
    }

    // Tank read with per-line modulation (one LFO, phase-shifted per line).
    const auto scale = sizeScale.next();
    const auto depth = modulationDepth.next() * (1.0f - currentFreeze);
    const auto lfoSin = lfo.sine();
    const auto lfoCos = lfo.cosine();

    std::array<float, kNumLines> tap {};
    std::array<float, kNumLines> feedback {};
    for (std::size_t i = 0; i < kNumLines; ++i)
    {
        const auto modulation = depth * (lfoSin * modulationCos[i] + lfoCos * modulationSin[i]);

        // A fractional read position low-passes the signal on every pass
        // through the line. Harmless while the tail decays, but it would drain
        // a frozen tail, so freezing glides each line onto an integer length,
        // where the cubic read is exact and lossless.
        auto length = baseLengthSamples[i] * scale;
        if (currentFreeze > 0.0f)
            length += currentFreeze * (std::round (length) - length);

        const auto position = std::clamp (length + modulation, 2.0f, maxReadDelay);
        tap[i] = lines[i].readCubic (position);
        feedback[i] = loopGains[i] * damping[i].process (tap[i]);
    }

    dsp::hadamardInPlace<kNumLines> (feedback.data());

    float sumL = 0.0f;
    float sumR = 0.0f;
    for (std::size_t i = 0; i < kNumLines; ++i)
    {
        const auto injected = (i % 2 == 0) ? input[0] * walshSign (i, kInputMaskL)
                                           : input[1] * walshSign (i, kInputMaskR);
        lines[i].write (feedback[i] + injected);

        sumL += walshSign (i, kOutputMaskL) * tap[i];
        sumR += walshSign (i, kOutputMaskR) * tap[i];
    }

    const auto l = sumL * kLineNormalisation * kOutputGain;
    const auto r = sumR * kLineNormalisation * kOutputGain;

    const auto w = width.next();
    const auto mid = 0.5f * (l + r);
    const auto side = 0.5f * (l - r) * w;
    outL = mid + side;
    outR = mid - side;

    lfo.advance();
}

void FdnReverb::process (const float* inL, const float* inR, float* wetL, float* wetR, int numSamples) noexcept
{
    const dsp::ScopedFlushDenormals flushDenormals;

    int done = 0;
    while (done < numSamples)
    {
        const auto block = std::min (kControlBlockSize, numSamples - done);

        // Control-rate parameters advance one block at a time.
        currentFreeze = freezeAmount.getCurrent();
        updateCoefficients();
        decaySeconds.skip (block);
        dampingRatio.skip (block);
        diffusionGain.skip (block);
        freezeAmount.skip (block);

        for (int i = done; i < done + block; ++i)
            processSample (inL[i], inR[i], wetL[i], wetR[i]);

        done += block;
    }
}

} // namespace echoform::engines
