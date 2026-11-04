#pragma once

// Numerical safety helpers for the audio path.
//
// Finiteness is tested on the IEEE-754 bit pattern rather than with
// std::isfinite so the checks keep working if the code is ever compiled with
// -ffast-math / /fp:fast (where the compiler may assume NaN/Inf never occur).

#include <cstdint>
#include <cstring>

namespace echoform::dsp
{

/// Intentional exact floating-point comparison (e.g. "is this already the
/// target?"). Wrapped so -Wfloat-equal stays enabled everywhere else.
#if defined(__GNUC__)
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wfloat-equal"
#endif
constexpr bool exactlyEqual (float a, float b) noexcept { return a == b; }
#if defined(__GNUC__)
    #pragma GCC diagnostic pop
#endif

inline bool isFinite (float x) noexcept
{
    std::uint32_t bits = 0;
    std::memcpy (&bits, &x, sizeof (bits));
    return (bits & 0x7f800000u) != 0x7f800000u;
}

/// Replaces every NaN/Inf sample with 0.
/// @returns the number of samples that had to be replaced.
inline int sanitize (float* samples, int numSamples) noexcept
{
    int replaced = 0;
    for (int i = 0; i < numSamples; ++i)
    {
        if (! isFinite (samples[i]))
        {
            samples[i] = 0.0f;
            ++replaced;
        }
    }
    return replaced;
}

} // namespace echoform::dsp
