#include "engines/ParameterMapping.h"

#include "engines/DelayEngine.h"

#include <algorithm>
#include <cmath>

namespace echoform::engines
{

using params::Param;

double resolveDelaySeconds (bool sync, float timeMs, int divisionIndex, double bpm) noexcept
{
    if (! sync)
        return static_cast<double> (timeMs) / 1000.0;

    const auto tempo = (std::isfinite (bpm) && bpm >= kMinBpm && bpm <= kMaxBpm) ? bpm : kDefaultBpm;
    const auto index = static_cast<std::size_t> (std::clamp (divisionIndex, 0, static_cast<int> (params::kTempoDivisions.size()) - 1));
    return params::kTempoDivisions[index].quarterNotes * 60.0 / tempo;
}

ChainSettings mapToChainSettings (const params::ParameterSnapshot& p, double bpm) noexcept
{
    ChainSettings c;
    c.routing = p.getChoiceAs<params::Routing> (Param::routing);

    c.delayEnabled = ! p.getBool (Param::delayBypass);
    c.delayMix = p.get (Param::delayMix) / 100.0f;
    c.delay.mode = p.getChoiceAs<params::DelayMode> (Param::delayMode);
    c.delay.timeSeconds = static_cast<float> (resolveDelaySeconds (p.getBool (Param::delaySync), p.get (Param::delayTime),
                                                                   p.getChoice (Param::delayDivision), bpm));
    c.delay.feedback = p.get (Param::delayFeedback) / 100.0f;
    c.delay.tone = p.get (Param::delayTone) / 100.0f;
    c.delay.modRateHz = p.get (Param::delayModRate);
    c.delay.modDepth = p.get (Param::delayModDepth) / 100.0f;
    c.delay.width = p.get (Param::delayWidth) / 100.0f;

    c.reverbEnabled = ! p.getBool (Param::reverbBypass);
    c.reverbMix = p.get (Param::reverbMix) / 100.0f;
    c.reverb.mode = p.getChoiceAs<params::ReverbMode> (Param::reverbMode);
    c.reverb.decaySeconds = p.get (Param::reverbDecay);
    c.reverb.size = p.get (Param::reverbSize) / 100.0f;
    c.reverb.preDelaySeconds = p.get (Param::reverbPreDelay) / 1000.0f;
    c.reverb.damping = p.get (Param::reverbDamping) / 100.0f;
    c.reverb.diffusion = p.get (Param::reverbDiffusion) / 100.0f;
    c.reverb.modRateHz = p.get (Param::reverbModRate);
    c.reverb.modDepth = p.get (Param::reverbModDepth) / 100.0f;
    c.reverb.width = p.get (Param::reverbWidth) / 100.0f;
    c.reverb.freeze = p.getBool (Param::reverbFreeze);

    return c;
}

double estimateTailSeconds (const ChainSettings& s) noexcept
{
    double delayTail = 0.0;
    if (s.delayEnabled && s.delayMix > 0.0f)
    {
        const auto time = std::clamp (static_cast<double> (s.delay.timeSeconds), DelayEngine::kMinDelaySeconds,
                                      DelayEngine::kMaxDelaySeconds);
        const auto g = std::clamp (static_cast<double> (s.delay.feedback), 0.0, 1.0);
        // Repeats until the echo is 60 dB down: g^k = 0.001.
        const auto repeats = g <= 0.001 ? 1.0 : (g >= 0.999 ? kMaxReportedTailSeconds / time : std::log (0.001) / std::log (g));
        delayTail = time * (1.0 + repeats);
    }

    double reverbTail = 0.0;
    if (s.reverbEnabled && s.reverbMix > 0.0f)
    {
        reverbTail = s.reverb.freeze ? kMaxReportedTailSeconds
                                     : static_cast<double> (s.reverb.decaySeconds) + static_cast<double> (s.reverb.preDelaySeconds);
    }

    const auto total = s.routing == params::Routing::Parallel ? std::max (delayTail, reverbTail) : delayTail + reverbTail;
    return std::min (total, kMaxReportedTailSeconds);
}

} // namespace echoform::engines
