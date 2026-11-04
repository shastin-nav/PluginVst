#pragma once

// Schroeder all-pass diffuser:  H(z) = (z^-D - g) / (1 - g z^-D)
// Flat magnitude response; smears transients in time. Lattice form:
//   v[n] = x[n] + g v[n-D],  y[n] = v[n-D] - g v[n]

#include "dsp/DelayLine.h"

namespace echoform::dsp
{

class SchroederAllpass
{
public:
    void prepare (int maxDelaySamples)
    {
        line.prepare (maxDelaySamples);
        delay = maxDelaySamples;
    }

    void reset() noexcept { line.reset(); }

    /// d in [1, max delay given to prepare()].
    void setDelay (int d) noexcept { delay = d < 1 ? 1 : (d > line.getMaxDelay() ? line.getMaxDelay() : d); }
    void setGain (float g) noexcept { gain = g; }

    float process (float x) noexcept
    {
        const auto delayed = line.readInteger (delay);
        const auto v = x + gain * delayed;
        line.write (v);
        return delayed - gain * v;
    }

private:
    DelayLine line;
    int delay = 1;
    float gain = 0.0f;
};

} // namespace echoform::dsp
