#pragma once

// A plain, fixed-size copy of every parameter value, taken once per audio block.
//
// The audio thread fills it from lock-free atomics (plugin/ParameterBridge) and
// hands it to the DSP layer, so DSP code never touches the plug-in framework.
// Holding *base* parameter values here (before macro offsets are applied)
// is what lets the macro system preserve the user's original settings.

#include "params/ParameterSpecs.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>

namespace echoform::params
{

class ParameterSnapshot
{
public:
    /// Deterministic initial state: every parameter at its default.
    ParameterSnapshot() noexcept
    {
        for (std::size_t i = 0; i < kNumParams; ++i)
            values[i] = kParamSpecs[i].defaultValue;
    }

    float get (Param p) const noexcept { return values[indexOf (p)]; }

    bool getBool (Param p) const noexcept { return get (p) >= 0.5f; }

    int getChoice (Param p) const noexcept
    {
        const auto& s = spec (p);
        const auto index = static_cast<int> (std::lround (get (p)));
        return std::clamp (index, 0, static_cast<int> (s.numChoices) - 1);
    }

    template <typename Enum>
    Enum getChoiceAs (Param p) const noexcept
    {
        return static_cast<Enum> (getChoice (p));
    }

    /// Stores a value, enforcing the spec's range. Non-finite input is replaced
    /// by the default so a corrupt host value can never reach the DSP.
    void set (Param p, float value) noexcept
    {
        const auto& s = spec (p);
        values[indexOf (p)] = isFiniteValue (value) ? std::clamp (value, s.minValue, s.maxValue)
                                                    : s.defaultValue;
    }

    bool operator== (const ParameterSnapshot& other) const noexcept { return values == other.values; }
    bool operator!= (const ParameterSnapshot& other) const noexcept { return ! (*this == other); }

private:
    static bool isFiniteValue (float x) noexcept
    {
        std::uint32_t bits = 0;
        std::memcpy (&bits, &x, sizeof (bits));
        return (bits & 0x7f800000u) != 0x7f800000u;
    }

    std::array<float, kNumParams> values {};
};

} // namespace echoform::params
