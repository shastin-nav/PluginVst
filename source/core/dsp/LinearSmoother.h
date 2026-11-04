#pragma once

// Linear parameter ramp used to remove zipper noise from control changes.
//
// Real-time safe: no allocation, no locks, O(1) per sample.
//
// Each ramp value is computed as start + increment * n rather than by
// repeated accumulation, so rounding error does not build up over long ramps
// and every step has the same size to within a few ULPs. The final sample of
// a ramp is exactly the target, so a settled smoother with target 1.0 is
// bit-transparent.

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

        if (rampLength <= 0)
        {
            snapTo (newTarget);
            return;
        }

        startValue = currentValue;
        targetValue = newTarget;
        increment = (targetValue - startValue) / static_cast<float> (rampLength);
        position = 0;
    }

    /// Jump immediately to `value`, cancelling any ramp.
    void snapTo (float value) noexcept
    {
        startValue = currentValue = targetValue = value;
        increment = 0.0f;
        position = rampLength;
    }

    /// Advance one sample and return the new value.
    float next() noexcept
    {
        if (position >= rampLength)
            return currentValue;

        ++position;
        currentValue = position == rampLength ? targetValue
                                              : startValue + increment * static_cast<float> (position);
        return currentValue;
    }

    /// Advance `numSamples` samples at once (used when a block is skipped).
    void skip (int numSamples) noexcept
    {
        if (position >= rampLength || numSamples <= 0)
            return;

        position = std::min (rampLength, position + numSamples);
        currentValue = position == rampLength ? targetValue
                                              : startValue + increment * static_cast<float> (position);
    }

    bool isSmoothing() const noexcept { return position < rampLength; }
    float getCurrent() const noexcept { return currentValue; }
    float getTarget() const noexcept { return targetValue; }
    int getRampLengthSamples() const noexcept { return rampLength; }

private:
    float startValue = 0.0f;
    float currentValue = 0.0f;
    float targetValue = 0.0f;
    float increment = 0.0f;
    int rampLength = 0;
    int position = 0;
};

} // namespace echoform::dsp
