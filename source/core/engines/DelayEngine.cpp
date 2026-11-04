#include "engines/DelayEngine.h"

#include "dsp/Denormals.h"
#include "dsp/FloatSafety.h"
#include "dsp/Saturation.h"

#include <algorithm>
#include <cmath>

namespace echoform::engines
{
namespace
{
    constexpr double kDarkestLowpassHz = 800.0;
    constexpr double kNeutralLowpassHz = 20000.0;
    constexpr double kBrightestHighpassHz = 1000.0;

    /// Below 0: low-pass from 20 kHz (tone 0) down to 800 Hz (tone -1).
    /// The coefficient fades in over tone [0, -0.05] so neutral is exactly flat.
    float lowpassCoefficientForTone (float tone, double sampleRate) noexcept
    {
        if (tone >= 0.0f)
            return 0.0f;

        const auto amount = static_cast<double> (std::min (1.0f, -tone));
        const auto cutoff = kNeutralLowpassHz * std::pow (kDarkestLowpassHz / kNeutralLowpassHz, amount);
        const auto fadeIn = std::min (1.0, amount / 0.05);
        return static_cast<float> (fadeIn) * dsp::onePoleCoefficient (cutoff, sampleRate);
    }

    /// Above 0: high-pass from 15 Hz (tone 0) up to 1 kHz (tone +1).
    float highpassCoefficientForTone (float tone, double sampleRate) noexcept
    {
        const auto amount = static_cast<double> (std::clamp (tone, 0.0f, 1.0f));
        const auto cutoff = DelayEngine::kBaseHighpassHz
                            * std::pow (kBrightestHighpassHz / DelayEngine::kBaseHighpassHz, amount);
        return dsp::onePoleCoefficient (cutoff, sampleRate);
    }
} // namespace

void DelayEngine::prepare (double newSampleRate)
{
    sampleRate = newSampleRate > 0.0 ? newSampleRate : 44100.0;

    maxModulationSamples = static_cast<float> (kMaxModulationSeconds * sampleRate);
    minReadDelay = std::max (2.0f, static_cast<float> (kMinDelaySeconds * sampleRate));
    maxReadDelay = static_cast<float> (kMaxDelaySeconds * sampleRate);

    const auto capacity = static_cast<int> (std::ceil (maxReadDelay + maxModulationSamples)) + 4;
    for (auto& line : lines)
        line.prepare (capacity);

    fadeIncrement = static_cast<float> (1.0 / (kTimeCrossfadeSeconds * sampleRate));

    feedback.reset (sampleRate, kSmoothingSeconds, 0.0f);
    modulationDepth.reset (sampleRate, kSmoothingSeconds, 0.0f);
    lowpassCoefficient.reset (sampleRate, kSmoothingSeconds, 0.0f);
    highpassCoefficient.reset (sampleRate, kSmoothingSeconds, 0.0f);
    crossFeed.reset (sampleRate, kSmoothingSeconds, 0.0f);
    width.reset (sampleRate, kSmoothingSeconds, 1.0f);

    reset();
}

void DelayEngine::reset() noexcept
{
    for (auto& line : lines)
        line.reset();
    for (auto& f : toneLowpass)
        f.reset();
    for (auto& f : toneHighpass)
        f.reset();

    lfo.reset();
    fading = false;
    fadePosition = 0.0f;
    snapOnNextSettings = true;
}

void DelayEngine::setSettings (const DelaySettings& s) noexcept
{
    if (sampleRate <= 0.0)
        return;

    const auto requestedDelay = static_cast<float> (static_cast<double> (s.timeSeconds) * sampleRate);
    targetDelay = dsp::isFinite (requestedDelay) ? std::clamp (requestedDelay, minReadDelay, maxReadDelay) : minReadDelay;

    const auto pingPong = s.mode == params::DelayMode::PingPong;

    feedback.setTarget (std::clamp (s.feedback, 0.0f, 1.0f));
    modulationDepth.setTarget (std::clamp (s.modDepth, 0.0f, 1.0f) * maxModulationSamples);
    lowpassCoefficient.setTarget (lowpassCoefficientForTone (s.tone, sampleRate));
    highpassCoefficient.setTarget (highpassCoefficientForTone (s.tone, sampleRate));
    crossFeed.setTarget (pingPong ? 1.0f : 0.0f);
    width.setTarget (std::clamp (s.width, 0.0f, 2.0f));
    lfo.setFrequency (static_cast<double> (std::max (0.0f, s.modRateHz)), sampleRate);

    if (snapOnNextSettings)
    {
        currentDelay = nextDelay = targetDelay;
        fading = false;
        feedback.snapTo (feedback.getTarget());
        modulationDepth.snapTo (modulationDepth.getTarget());
        lowpassCoefficient.snapTo (lowpassCoefficient.getTarget());
        highpassCoefficient.snapTo (highpassCoefficient.getTarget());
        crossFeed.snapTo (crossFeed.getTarget());
        width.snapTo (width.getTarget());
        snapOnNextSettings = false;
    }
}

float DelayEngine::readTap (std::size_t channel, float delaySamples, float modulation) const noexcept
{
    // Never let modulation push the read position below the interpolation
    // minimum or beyond the buffer.
    const auto position = std::clamp (delaySamples + modulation, minReadDelay, maxReadDelay + maxModulationSamples);
    return lines[channel].readCubic (position);
}

void DelayEngine::process (const float* inL, const float* inR, float* wetL, float* wetR, int numSamples) noexcept
{
    const dsp::ScopedFlushDenormals flushDenormals;

    for (int i = 0; i < numSamples; ++i)
    {
        if (! fading && ! dsp::exactlyEqual (targetDelay, currentDelay))
        {
            nextDelay = targetDelay;
            fadePosition = 0.0f;
            fading = true;
        }

        const auto loopGain = feedback.next();
        const auto x = crossFeed.next();

        // Modulation depth is limited to the room available below the delay.
        const auto depth = std::min (modulationDepth.next(), 0.5f * std::max (0.0f, currentDelay - minReadDelay));
        const auto modL = depth * lfo.sine();
        const auto modR = depth * lfo.cosine();

        auto yL = readTap (0, currentDelay, modL);
        auto yR = readTap (1, currentDelay, modR);

        if (fading)
        {
            const auto nextDepthLimit = 0.5f * std::max (0.0f, nextDelay - minReadDelay);
            const auto nextScale = depth > 0.0f ? std::min (1.0f, nextDepthLimit / depth) : 0.0f;
            const auto bL = readTap (0, nextDelay, modL * nextScale);
            const auto bR = readTap (1, nextDelay, modR * nextScale);
            yL += fadePosition * (bL - yL);
            yR += fadePosition * (bR - yR);

            fadePosition += fadeIncrement;
            if (fadePosition >= 1.0f)
            {
                currentDelay = nextDelay;
                fading = false;
            }
        }

        // Tone, inside the loop (every repeat is filtered again).
        const auto lpA = lowpassCoefficient.next();
        const auto hpA = highpassCoefficient.next();
        for (std::size_t ch = 0; ch < 2; ++ch)
        {
            toneLowpass[ch].setCoefficient (lpA);
            toneHighpass[ch].setCoefficient (hpA);
        }
        yL = toneHighpass[0].process (toneLowpass[0].process (yL));
        yR = toneHighpass[1].process (toneLowpass[1].process (yR));

        // Stereo cross-feed and input routing (Ping Pong: mono input, left only).
        const auto feedbackL = yL + x * (yR - yL);
        const auto feedbackR = yR + x * (yL - yR);
        const auto monoIn = 0.5f * (inL[i] + inR[i]);
        const auto sendL = inL[i] + x * (monoIn - inL[i]);
        const auto sendR = inR[i] * (1.0f - x);

        lines[0].write (dsp::softClip (sendL + loopGain * feedbackL));
        lines[1].write (dsp::softClip (sendR + loopGain * feedbackR));

        // Wet output with mid/side width.
        const auto w = width.next();
        const auto mid = 0.5f * (yL + yR);
        const auto side = 0.5f * (yL - yR) * w;
        wetL[i] = mid + side;
        wetR[i] = mid - side;

        lfo.advance();
    }
}

} // namespace echoform::engines
