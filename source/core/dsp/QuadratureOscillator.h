#pragma once

// Sine/cosine LFO by phasor rotation: two multiplies and adds per sample, no
// trigonometric calls on the audio thread. Amplitude is renormalised every
// sample (first-order correction), so it stays at 1 indefinitely.
//
// Any phase-shifted copy is available cheaply:
//   sin(theta + phi) = sine() * cos(phi) + cosine() * sin(phi)

#include "dsp/Filters.h"

#include <cmath>

namespace echoform::dsp
{

class QuadratureOscillator
{
public:
    /// Phase-continuous frequency change (allocation-free; calls cos/sin once).
    void setFrequency (double hz, double sampleRate) noexcept
    {
        const auto w = sampleRate > 0.0 ? kTwoPi * hz / sampleRate : 0.0;
        cosStep = static_cast<float> (std::cos (w));
        sinStep = static_cast<float> (std::sin (w));
    }

    void reset (double phaseRadians = 0.0) noexcept
    {
        c = static_cast<float> (std::cos (phaseRadians));
        s = static_cast<float> (std::sin (phaseRadians));
    }

    void advance() noexcept
    {
        const auto nextC = c * cosStep - s * sinStep;
        const auto nextS = s * cosStep + c * sinStep;
        const auto correction = 1.5f - 0.5f * (nextC * nextC + nextS * nextS);
        c = nextC * correction;
        s = nextS * correction;
    }

    float sine() const noexcept { return s; }
    float cosine() const noexcept { return c; }

private:
    float c = 1.0f;
    float s = 0.0f;
    float cosStep = 1.0f;
    float sinStep = 0.0f;
};

} // namespace echoform::dsp
