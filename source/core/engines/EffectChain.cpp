#include "engines/EffectChain.h"

#include "dsp/Denormals.h"
#include "dsp/FloatSafety.h"

#include <algorithm>
#include <cmath>

namespace echoform::engines
{
namespace
{
    constexpr float kHalfPi = 1.57079632679489661923f;

    /// Equal-power law with exact end points: m = 0 -> (1, 0), m = 1 -> (0, 1).
    float dryGainForMix (float mix) noexcept { return mix <= 0.0f ? 1.0f : (mix >= 1.0f ? 0.0f : std::sin ((1.0f - mix) * kHalfPi)); }
    float wetGainForMix (float mix) noexcept { return mix <= 0.0f ? 0.0f : (mix >= 1.0f ? 1.0f : std::sin (mix * kHalfPi)); }

    enum ScratchSlot
    {
        engineInL,
        engineInR,
        engineWetL,
        engineWetR,
        parallelWetL,
        parallelWetR,
        numScratchSlots
    };
} // namespace

void EffectChain::prepare (double newSampleRate)
{
    sampleRate = newSampleRate > 0.0 ? newSampleRate : 44100.0;

    delay.prepare (sampleRate);
    reverb.prepare (sampleRate);
    scratch.assign (static_cast<std::size_t> (numScratchSlots * kBlockSize), 0.0f);

    for (auto* gains : { &delayGains, &reverbGains })
    {
        gains->active.reset (sampleRate, kGainSmoothingSeconds, 0.0f);
        gains->dry.reset (sampleRate, kGainSmoothingSeconds, 1.0f);
        gains->wet.reset (sampleRate, kGainSmoothingSeconds, 0.0f);
    }
    routingGain.reset (sampleRate, kRoutingFadeSeconds, 1.0f);

    reset();
}

void EffectChain::reset() noexcept
{
    delay.reset();
    reverb.reset();
    snapOnNextSettings = true;
}

void EffectChain::setStageTargets (StageGains& gains, bool enabled, float mix) noexcept
{
    const auto m = dsp::isFinite (mix) ? std::clamp (mix, 0.0f, 1.0f) : 0.0f;
    gains.active.setTarget (enabled ? 1.0f : 0.0f);
    gains.dry.setTarget (enabled ? dryGainForMix (m) : 1.0f);
    gains.wet.setTarget (enabled ? wetGainForMix (m) : 0.0f);
}

void EffectChain::setSettings (const ChainSettings& settings) noexcept
{
    if (sampleRate <= 0.0)
        return;

    delay.setSettings (settings.delay);
    reverb.setSettings (settings.reverb);
    setStageTargets (delayGains, settings.delayEnabled, settings.delayMix);
    setStageTargets (reverbGains, settings.reverbEnabled, settings.reverbMix);
    requestedRouting = settings.routing;

    if (snapOnNextSettings)
    {
        for (auto* gains : { &delayGains, &reverbGains })
        {
            gains->active.snapTo (gains->active.getTarget());
            gains->dry.snapTo (gains->dry.getTarget());
            gains->wet.snapTo (gains->wet.getTarget());
        }
        activeRouting = requestedRouting;
        routingGain.snapTo (1.0f);
        snapOnNextSettings = false;
    }
}

void EffectChain::process (float* left, float* right, int numSamples) noexcept
{
    if (scratch.empty()) // not prepared: pass through
        return;

    const dsp::ScopedFlushDenormals flushDenormals;

    for (int done = 0; done < numSamples;)
    {
        const auto chunk = std::min (kBlockSize, numSamples - done);
        processChunk (left + done, right + done, chunk);
        done += chunk;
    }
}

void EffectChain::processSilence (int numSamples) noexcept
{
    if (scratch.empty())
        return;

    const dsp::ScopedFlushDenormals flushDenormals;
    auto* silenceL = scratch.data() + engineInL * kBlockSize;
    auto* silenceR = scratch.data() + engineInR * kBlockSize;
    auto* wetL = scratch.data() + engineWetL * kBlockSize;
    auto* wetR = scratch.data() + engineWetR * kBlockSize;

    for (int done = 0; done < numSamples;)
    {
        const auto chunk = std::min (kBlockSize, numSamples - done);
        std::fill (silenceL, silenceL + chunk, 0.0f);
        std::fill (silenceR, silenceR + chunk, 0.0f);
        delay.process (silenceL, silenceR, wetL, wetR, chunk);
        reverb.process (silenceL, silenceR, wetL, wetR, chunk);
        done += chunk;
    }
}

void EffectChain::runEngine (bool isDelay, const float* inL, const float* inR, float* wetL, float* wetR, int numSamples) noexcept
{
    if (isDelay)
        delay.process (inL, inR, wetL, wetR, numSamples);
    else
        reverb.process (inL, inR, wetL, wetR, numSamples);
}

void EffectChain::runStage (StageGains& gains, bool isDelay, float* left, float* right, int numSamples) noexcept
{
    auto* inL = scratch.data() + engineInL * kBlockSize;
    auto* inR = scratch.data() + engineInR * kBlockSize;
    auto* wetL = scratch.data() + engineWetL * kBlockSize;
    auto* wetR = scratch.data() + engineWetR * kBlockSize;

    for (int i = 0; i < numSamples; ++i)
    {
        const auto a = gains.active.next();
        inL[i] = left[i] * a;
        inR[i] = right[i] * a;
    }

    runEngine (isDelay, inL, inR, wetL, wetR, numSamples);

    for (int i = 0; i < numSamples; ++i)
    {
        const auto dry = gains.dry.next();
        const auto wet = gains.wet.next();
        left[i] = dry * left[i] + wet * wetL[i];
        right[i] = dry * right[i] + wet * wetR[i];
    }
}

void EffectChain::processChunk (float* left, float* right, int numSamples) noexcept
{
    // Routing change: fade out, switch while silent, fade back in.
    if (requestedRouting != activeRouting && ! routingGain.isSmoothing())
        routingGain.setTarget (0.0f);

    switch (activeRouting)
    {
        case params::Routing::DelayToReverb:
            runStage (delayGains, true, left, right, numSamples);
            runStage (reverbGains, false, left, right, numSamples);
            break;

        case params::Routing::ReverbToDelay:
            runStage (reverbGains, false, left, right, numSamples);
            runStage (delayGains, true, left, right, numSamples);
            break;

        case params::Routing::Parallel:
        {
            auto* inL = scratch.data() + engineInL * kBlockSize;
            auto* inR = scratch.data() + engineInR * kBlockSize;
            auto* delayWetL = scratch.data() + engineWetL * kBlockSize;
            auto* delayWetR = scratch.data() + engineWetR * kBlockSize;
            auto* reverbWetL = scratch.data() + parallelWetL * kBlockSize;
            auto* reverbWetR = scratch.data() + parallelWetR * kBlockSize;

            for (int i = 0; i < numSamples; ++i)
            {
                const auto a = delayGains.active.next();
                inL[i] = left[i] * a;
                inR[i] = right[i] * a;
            }
            delay.process (inL, inR, delayWetL, delayWetR, numSamples);

            for (int i = 0; i < numSamples; ++i)
            {
                const auto a = reverbGains.active.next();
                inL[i] = left[i] * a;
                inR[i] = right[i] * a;
            }
            reverb.process (inL, inR, reverbWetL, reverbWetR, numSamples);

            for (int i = 0; i < numSamples; ++i)
            {
                const auto dry = delayGains.dry.next() * reverbGains.dry.next();
                const auto wetD = delayGains.wet.next();
                const auto wetR = reverbGains.wet.next();
                left[i] = dry * left[i] + wetD * delayWetL[i] + wetR * reverbWetL[i];
                right[i] = dry * right[i] + wetD * delayWetR[i] + wetR * reverbWetR[i];
            }
            break;
        }
    }

    // Apply the routing fade only while one is in progress (keeps the steady
    // state bit-exact).
    if (routingGain.isSmoothing() || routingGain.getCurrent() < 1.0f)
    {
        for (int i = 0; i < numSamples; ++i)
        {
            const auto g = routingGain.next();
            left[i] *= g;
            right[i] *= g;

            if (g <= 0.0f && requestedRouting != activeRouting)
            {
                // The rest of this chunk was rendered with the old routing:
                // keep it silent and fade the new routing in from the next chunk.
                std::fill (left + i + 1, left + numSamples, 0.0f);
                std::fill (right + i + 1, right + numSamples, 0.0f);
                activeRouting = requestedRouting;
                routingGain.setTarget (1.0f);
                break;
            }
        }
    }
}

} // namespace echoform::engines
