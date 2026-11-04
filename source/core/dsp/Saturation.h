#pragma once

// Soft clipping for feedback paths.

#include <cmath>

namespace echoform::dsp
{

/// Identity for |x| <= threshold; above it the curve bends smoothly
/// (continuous value and slope) towards `ceiling`, which it never exceeds.
/// Odd-symmetric and monotonic. Keeps a feedback loop bounded even at unity
/// loop gain, while leaving normal signal levels untouched.
inline float softClip (float x, float threshold = 1.0f, float ceiling = 2.0f) noexcept
{
    const auto magnitude = std::abs (x);
    if (magnitude <= threshold)
        return x;

    const auto range = ceiling - threshold;
    const auto shaped = threshold + range * std::tanh ((magnitude - threshold) / range);
    return std::copysign (shaped, x);
}

} // namespace echoform::dsp
