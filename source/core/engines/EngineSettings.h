#pragma once

// Engine-facing settings, in DSP-friendly units (seconds, 0..1 fractions).
//
// These are *effective* values. They are produced from the user's base
// parameter values by mapToChainSettings() (ParameterMapping.h); the macro
// system (P2) will apply its offsets there, never to the parameters
// themselves.

#include "params/ParameterSpecs.h"

namespace echoform::engines
{

struct DelaySettings
{
    params::DelayMode mode = params::DelayMode::Digital;
    float timeSeconds = 0.375f; ///< Already resolved from tempo sync.
    float feedback = 0.35f;     ///< Loop gain, 0..1.
    float tone = 0.0f;          ///< -1 (dark) .. 0 (neutral) .. +1 (bright).
    float modRateHz = 0.5f;
    float modDepth = 0.1f;      ///< 0..1 of the maximum excursion.
    float width = 1.0f;         ///< 0 (mono) .. 1 (natural) .. 2 (extra wide).
};

struct ReverbSettings
{
    params::ReverbMode mode = params::ReverbMode::Hall;
    float decaySeconds = 2.5f;  ///< RT60 at low frequencies.
    float size = 0.5f;          ///< 0..1, scales the delay-line lengths.
    float preDelaySeconds = 0.01f;
    float damping = 0.4f;       ///< 0..1: high-frequency RT60 shortening.
    float diffusion = 0.7f;     ///< 0..1: input all-pass diffusion.
    float modRateHz = 0.4f;
    float modDepth = 0.2f;      ///< 0..1 of the maximum excursion.
    float width = 1.0f;         ///< 0 (mono) .. 1 (natural) .. 2 (extra wide).
    bool freeze = false;
};

struct ChainSettings
{
    params::Routing routing = params::Routing::DelayToReverb;
    bool delayEnabled = true;
    float delayMix = 0.3f;      ///< 0..1 equal-power wet/dry.
    bool reverbEnabled = true;
    float reverbMix = 0.25f;    ///< 0..1 equal-power wet/dry.
    DelaySettings delay;
    ReverbSettings reverb;
};

} // namespace echoform::engines
