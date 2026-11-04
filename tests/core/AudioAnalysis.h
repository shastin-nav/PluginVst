#pragma once

// Measurement helpers for DSP tests (double precision, test-only).

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <numeric>
#include <random>
#include <vector>

namespace echoform::test
{

inline constexpr double kPi = 3.14159265358979323846;

using Signal = std::vector<float>;

inline Signal impulse (std::size_t length, float amplitude = 1.0f)
{
    Signal s (length, 0.0f);
    if (length > 0)
        s[0] = amplitude;
    return s;
}

inline Signal whiteNoise (std::size_t length, unsigned seed, float amplitude = 1.0f)
{
    std::mt19937 rng { seed };
    std::uniform_real_distribution<float> dist { -amplitude, amplitude };
    Signal s (length);
    for (auto& x : s)
        x = dist (rng);
    return s;
}

inline Signal sine (std::size_t length, double hz, double sampleRate, float amplitude = 1.0f)
{
    Signal s (length);
    for (std::size_t n = 0; n < length; ++n)
        s[n] = amplitude * static_cast<float> (std::sin (2.0 * kPi * hz * static_cast<double> (n) / sampleRate));
    return s;
}

inline double rms (const Signal& s, std::size_t begin = 0, std::size_t end = SIZE_MAX)
{
    end = std::min (end, s.size());
    if (end <= begin)
        return 0.0;
    double sum = 0.0;
    for (auto n = begin; n < end; ++n)
        sum += static_cast<double> (s[n]) * static_cast<double> (s[n]);
    return std::sqrt (sum / static_cast<double> (end - begin));
}

inline double toDb (double linear) { return 20.0 * std::log10 (std::max (linear, 1.0e-30)); }

inline float peakAbs (const Signal& s, std::size_t begin = 0, std::size_t end = SIZE_MAX)
{
    end = std::min (end, s.size());
    float peak = 0.0f;
    for (auto n = begin; n < end; ++n)
        peak = std::max (peak, std::abs (s[n]));
    return peak;
}

inline std::size_t peakIndex (const Signal& s, std::size_t begin = 0, std::size_t end = SIZE_MAX)
{
    end = std::min (end, s.size());
    std::size_t best = begin;
    for (auto n = begin; n < end; ++n)
        if (std::abs (s[n]) > std::abs (s[best]))
            best = n;
    return best;
}

inline bool allFinite (const Signal& s)
{
    return std::all_of (s.begin(), s.end(), [] (float x) { return std::isfinite (x); });
}

/// Largest absolute difference between consecutive samples.
inline float maxStep (const Signal& s, std::size_t begin = 1, std::size_t end = SIZE_MAX)
{
    end = std::min (end, s.size());
    float step = 0.0f;
    for (auto n = std::max<std::size_t> (begin, 1); n < end; ++n)
        step = std::max (step, std::abs (s[n] - s[n - 1]));
    return step;
}

/// Pearson correlation of two equally long signals over [begin, end).
inline double correlation (const Signal& a, const Signal& b, std::size_t begin, std::size_t end)
{
    double ab = 0.0, aa = 0.0, bb = 0.0;
    for (auto n = begin; n < std::min ({ end, a.size(), b.size() }); ++n)
    {
        ab += static_cast<double> (a[n]) * static_cast<double> (b[n]);
        aa += static_cast<double> (a[n]) * static_cast<double> (a[n]);
        bb += static_cast<double> (b[n]) * static_cast<double> (b[n]);
    }
    return aa > 0.0 && bb > 0.0 ? ab / std::sqrt (aa * bb) : 0.0;
}

/// RBJ biquad (double precision) for band-limiting measurements.
class Biquad
{
public:
    static Biquad lowpass (double hz, double sampleRate, double q = 0.7071) { return make (hz, sampleRate, q, false); }
    static Biquad highpass (double hz, double sampleRate, double q = 0.7071) { return make (hz, sampleRate, q, true); }

    Signal process (const Signal& in)
    {
        Signal out (in.size());
        for (std::size_t n = 0; n < in.size(); ++n)
        {
            const auto x = static_cast<double> (in[n]);
            const auto y = b0 * x + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
            x2 = x1;
            x1 = x;
            y2 = y1;
            y1 = y;
            out[n] = static_cast<float> (y);
        }
        return out;
    }

private:
    static Biquad make (double hz, double sampleRate, double q, bool high)
    {
        const auto w = 2.0 * kPi * hz / sampleRate;
        const auto alpha = std::sin (w) / (2.0 * q);
        const auto c = std::cos (w);
        const auto a0 = 1.0 + alpha;
        Biquad f;
        f.b0 = (high ? (1.0 + c) : (1.0 - c)) / 2.0 / a0;
        f.b1 = (high ? -(1.0 + c) : (1.0 - c)) / a0;
        f.b2 = f.b0;
        f.a1 = -2.0 * c / a0;
        f.a2 = (1.0 - alpha) / a0;
        return f;
    }

    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    double x1 = 0, x2 = 0, y1 = 0, y2 = 0;
};

/// RT60 from an impulse response by Schroeder backward integration with a
/// least-squares fit of the energy decay curve between -5 and -35 dB (T30).
/// Returns a negative value if the decay never reaches -35 dB.
inline double estimateRt60 (const Signal& left, const Signal& right, double sampleRate)
{
    const auto n = std::min (left.size(), right.size());
    std::vector<double> edc (n + 1, 0.0);
    for (std::size_t i = n; i-- > 0;)
        edc[i] = edc[i + 1] + static_cast<double> (left[i]) * left[i] + static_cast<double> (right[i]) * right[i];
    if (edc[0] <= 0.0)
        return -1.0;

    std::size_t start = n, stop = n;
    for (std::size_t i = 0; i < n; ++i)
    {
        const auto db = 10.0 * std::log10 (std::max (edc[i] / edc[0], 1.0e-30));
        if (start == n && db <= -5.0)
            start = i;
        if (db <= -35.0)
        {
            stop = i;
            break;
        }
    }
    if (start >= stop || stop == n)
        return -1.0;

    double sx = 0, sy = 0, sxx = 0, sxy = 0;
    const auto count = static_cast<double> (stop - start);
    for (auto i = start; i < stop; ++i)
    {
        const auto x = static_cast<double> (i) / sampleRate;
        const auto y = 10.0 * std::log10 (edc[i] / edc[0]);
        sx += x;
        sy += y;
        sxx += x * x;
        sxy += x * y;
    }
    const auto slope = (count * sxy - sx * sy) / (count * sxx - sx * sx); // dB per second
    return slope < 0.0 ? -60.0 / slope : -1.0;
}

} // namespace echoform::test
