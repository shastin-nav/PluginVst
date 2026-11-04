#pragma once

// In-place normalised fast Walsh-Hadamard transform: x <- H x / sqrt(N).
// The normalised Hadamard matrix is orthogonal (energy preserving) and its
// own inverse, which makes it a lossless, maximally mixing FDN feedback
// matrix at O(N log N) cost.

#include <cstddef>

namespace echoform::dsp
{

/// 1/sqrt(n) for a power of two n, evaluated at compile time (a function-local
/// static would be initialised behind a lock on first use, possibly on the
/// audio thread).
constexpr double inverseSqrtOfPowerOfTwo (std::size_t n) noexcept
{
    double result = 1.0;
    while (n >= 4)
    {
        n /= 4;
        result *= 0.5;
    }
    return n == 2 ? result * 0.70710678118654752440 : result;
}

template <std::size_t N>
inline void hadamardInPlace (float* x) noexcept
{
    static_assert (N >= 2 && (N & (N - 1)) == 0, "N must be a power of two");

    for (std::size_t half = 1; half < N; half *= 2)
    {
        for (std::size_t start = 0; start < N; start += 2 * half)
        {
            for (std::size_t i = start; i < start + half; ++i)
            {
                const auto a = x[i];
                const auto b = x[i + half];
                x[i] = a + b;
                x[i + half] = a - b;
            }
        }
    }

    // For N = 4^k this is an exact power of two, so scaling adds no rounding.
    constexpr auto scale = static_cast<float> (inverseSqrtOfPowerOfTwo (N));
    for (std::size_t i = 0; i < N; ++i)
        x[i] *= scale;
}

} // namespace echoform::dsp
