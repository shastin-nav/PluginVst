#pragma once

// Decibel <-> linear gain conversion for the audio path.

#include <cmath>

namespace echoform::dsp
{

/// Gains within this distance of 0 dB are treated as exactly unity.
///
/// Why: host parameters travel through a normalised 0..1 representation. For
/// a range such as -48..+12 dB, 0 dB normalises to 0.8, which float cannot
/// represent exactly. Converting back computes -48 + 0.8f * 60. On x86 the
/// product rounds to exactly 48 and the result is 0 dB; ARM compilers fuse it
/// into one FMA, skipping that rounding and leaving 7.15e-7 dB (verified on
/// Apple Silicon CI). Without this tolerance a plug-in set to 0 dB would not
/// be bit-transparent on some platforms. 0.001 dB is a 0.0115 % amplitude
/// change, far below audibility.
inline constexpr float kUnityToleranceDb = 1.0e-3f;

inline float decibelsToGain (float decibels) noexcept
{
    if (std::abs (decibels) < kUnityToleranceDb)
        return 1.0f;

    return std::pow (10.0f, decibels * 0.05f);
}

} // namespace echoform::dsp
