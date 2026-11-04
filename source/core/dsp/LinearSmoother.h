#pragma once

// Linear parameter ramp used to remove zipper noise from control changes.
//
// Real-time safe: no allocation, no locks, O(1) per sample. When a ramp
// completes the output is set exactly to the target, so a settled smoother
// with target 1.0 is bit-transparent.

#include "dsp/FloatSafety.h"

#include <algorithm>
#include <cmath>

namespace echoform::dsp
{

class LinearSmoother
{
public:
    /// Configure ramp length and jump to `initialValue` without ramping.
    void reset (double sampleRate, double rampSeconds, float initialValue) noexcept
    {
        const auto samples = std::max (0.0, std::floor (sampleRate * rampSeconds));
        rampLength = static_cast<int> (std::min (samples, 1.0e8));
        snapTo (initialValue);
    }

    /// Start a ramp from the current value to `newTarget`. Re-targeting to the
    /// value already being approached does not restart the ramp.
    void setTarget (float newTarget) noexcept
    {
        if (exactlyEqual (newTarget, targetValue))
            return;

        targetValue = newTarget;

        if (rampLength <= 0)
        {
            snapTo (newTarget);
            return;
        }

        remaining = rampLength;
        increment = (targetValue - currentValue) / static_cast<float> (rampLength);
    }

    /// Jump immediately to `value`, cancelling any ramp.
    void snapTo (float value) noexcept
    {
        currentValue = targetValue = value;
        increment = 0.0f;
        remaining = 0;
    }

    /// Advance one sample and return the new value.
    float next() noexcept
    {
        if (remaining <= 0)
            return currentValue;

        if (--remaining == 0)
            currentValue = targetValue;
        else
            currentValue += increment;

        return currentValue;
    }

    /// Advance `numSamples` samples at once (used when a block is skipped).
    void skip (int numSamples) noexcept
    {
        if (remaining <= 0 || numSamples <= 0)
            return;

        if (numSamples >= remaining)
        {
            snapTo (targetValue);
            return;
        }

        remaining -= numSamples;
        currentValue += increment * static_cast<float> (numSamples);
    }

    bool isSmoothing() const noexcept { return remaining > 0; }
    float getCurrent() const noexcept { return currentValue; }
    float getTarget() const noexcept { return targetValue; }
    int getRampLengthSamples() const noexcept { return rampLength; }

private:
    float currentValue = 0.0f;
    float targetValue = 0.0f;
    float increment = 0.0f;
    int rampLength = 0;
    int remaining = 0;
};

} // namespace echoform::dsp
