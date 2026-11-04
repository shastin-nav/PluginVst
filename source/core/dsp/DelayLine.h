#pragma once

// Single-channel circular delay buffer with integer, linear and cubic reads.
//
// Storage is allocated once in prepare() (not real-time safe); every other
// member is allocation-free and O(1).
//
// Convention (read-before-write): within one sample step, read first, then
// write. read*(d) returns the sample written d steps ago, so reading d then
// writing gives a delay of exactly d samples (d >= 1).

#include <cstddef>
#include <vector>

namespace echoform::dsp
{

class DelayLine
{
public:
    /// Allocates for delays up to `maxDelaySamples` (plus interpolation margin)
    /// and clears the buffer.
    void prepare (int maxDelaySamples)
    {
        maxDelay = maxDelaySamples < 1 ? 1 : maxDelaySamples;

        std::size_t capacity = 4;
        while (capacity < static_cast<std::size_t> (maxDelay) + 4)
            capacity <<= 1;

        buffer.assign (capacity, 0.0f);
        mask = capacity - 1;
        writeIndex = 0;
    }

    /// Clears the stored audio. Allocation-free.
    void reset() noexcept
    {
        for (auto& s : buffer)
            s = 0.0f;
        writeIndex = 0;
    }

    int getMaxDelay() const noexcept { return maxDelay; }
    std::size_t getCapacity() const noexcept { return buffer.size(); }

    void write (float x) noexcept
    {
        buffer[writeIndex] = x;
        writeIndex = (writeIndex + 1) & mask;
    }

    /// Sample written `d` steps ago, d in [1, maxDelay].
    float readInteger (int d) const noexcept { return buffer[(writeIndex - static_cast<std::size_t> (d)) & mask]; }

    /// Linear interpolation, d in [1, maxDelay].
    float readLinear (float d) const noexcept
    {
        const auto whole = static_cast<int> (d);
        const auto frac = d - static_cast<float> (whole);
        const auto a = readInteger (whole);
        const auto b = readInteger (whole + 1);
        return a + frac * (b - a);
    }

    /// 4-point cubic Hermite (Catmull-Rom) interpolation, d in [2, maxDelay].
    /// Its magnitude response never exceeds unity for any fractional position,
    /// so it cannot inject energy into a feedback loop.
    float readCubic (float d) const noexcept
    {
        const auto whole = static_cast<int> (d);
        const auto f = d - static_cast<float> (whole);

        const auto xm1 = readInteger (whole - 1); // newer neighbour
        const auto x0 = readInteger (whole);
        const auto x1 = readInteger (whole + 1);
        const auto x2 = readInteger (whole + 2); // older neighbour

        const auto c1 = 0.5f * (x1 - xm1);
        const auto c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2;
        const auto c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
        return ((c3 * f + c2) * f + c1) * f + x0;
    }

private:
    std::vector<float> buffer;
    std::size_t mask = 0;
    std::size_t writeIndex = 0;
    int maxDelay = 0;
};

} // namespace echoform::dsp
