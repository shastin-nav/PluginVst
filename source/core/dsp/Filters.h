#pragma once

// Small real-time-safe filters used inside feedback loops.

#include <cmath>

namespace echoform::dsp
{

inline constexpr double kTwoPi = 6.283185307179586476925286766559;

/// Pole radius of a one-pole filter with the given -3 dB-ish cutoff.
inline float onePoleCoefficient (double cutoffHz, double sampleRate) noexcept
{
    if (cutoffHz <= 0.0 || sampleRate <= 0.0)
        return 0.0f;
    return static_cast<float> (std::exp (-kTwoPi * cutoffHz / sampleRate));
}

/// y[n] = (1 - a) x[n] + a y[n-1].  Unity gain at DC; a = 0 is a pass-through.
/// Gain at Nyquist is (1 - a) / (1 + a).
class OnePoleLowpass
{
public:
    void setCoefficient (float a) noexcept { coefficient = a; }
    float getCoefficient() const noexcept { return coefficient; }
    void reset() noexcept { state = 0.0f; }

    float process (float x) noexcept
    {
        state = x + coefficient * (state - x);
        return state;
    }

private:
    float coefficient = 0.0f;
    float state = 0.0f;
};

/// One-pole high-pass formed as x - lowpass(x). Coefficient a = 0 removes
/// everything; use onePoleCoefficient(cutoff) for a high-pass at `cutoff`.
class OnePoleHighpass
{
public:
    void setCoefficient (float a) noexcept { lowpass.setCoefficient (a); }
    void reset() noexcept { lowpass.reset(); }
    float process (float x) noexcept { return x - lowpass.process (x); }

private:
    OnePoleLowpass lowpass;
};

/// DC blocker: y[n] = x[n] - x[n-1] + r y[n-1]. Zero at DC, pole at r.
class DcBlocker
{
public:
    void setCutoff (double cutoffHz, double sampleRate) noexcept { r = onePoleCoefficient (cutoffHz, sampleRate); }
    void reset() noexcept { previousInput = previousOutput = 0.0f; }

    float process (float x) noexcept
    {
        const auto y = x - previousInput + r * previousOutput;
        previousInput = x;
        previousOutput = y;
        return y;
    }

private:
    float r = 0.999f;
    float previousInput = 0.0f;
    float previousOutput = 0.0f;
};

} // namespace echoform::dsp
