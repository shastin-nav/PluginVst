#pragma once

// Parameter values (natural units) -> engine settings.
//
// This is the single place where user-facing parameters become DSP settings.
// Macros (P2) will be applied here as offsets on top of the base values; the
// snapshot passed in is never modified.

#include "engines/EngineSettings.h"
#include "params/ParameterSnapshot.h"

namespace echoform::engines
{

inline constexpr double kDefaultBpm = 120.0;
inline constexpr double kMinBpm = 20.0;
inline constexpr double kMaxBpm = 999.0;

/// Upper bound reported to hosts as the tail length.
inline constexpr double kMaxReportedTailSeconds = 60.0;

/// Delay time in seconds: the note division at `bpm` when synced, otherwise
/// the free time. A non-finite or out-of-range BPM falls back to 120.
double resolveDelaySeconds (bool sync, float timeMs, int divisionIndex, double bpm) noexcept;

ChainSettings mapToChainSettings (const params::ParameterSnapshot& snapshot, double bpm) noexcept;

/// Conservative tail length (seconds) for hosts: time for the output to fall
/// ~60 dB after the input stops. Capped at kMaxReportedTailSeconds (also used
/// while the reverb is frozen).
double estimateTailSeconds (const ChainSettings& settings) noexcept;

} // namespace echoform::engines
