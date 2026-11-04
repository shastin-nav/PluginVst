#pragma once

// Single source of truth for every ECHOFORM parameter.
//
// Framework independent: the JUCE layer (plugin/ParameterLayout.cpp) builds the
// host-facing parameters from this table, and the DSP layer reads values through
// ParameterSnapshot, which is indexed by the same Param enum.
//
// Compatibility rules (hosts persist parameter IDs and ranges in sessions):
//   * Never rename or remove an ID once released.
//   * Never change a range or the meaning of a value without bumping the
//     state schema (plugin/StateSerializer.h) and adding a migration.
//   * New parameters are appended at the end of the Param enum.
//
// Values are stored in natural units (dB, ms, s, Hz, %), not normalised 0..1.

#include <array>
#include <cstddef>

namespace echoform::params
{

/// Version hint passed to juce::ParameterID. Increment only for parameters
/// added in a later release (AU uses it to keep parameter order stable).
inline constexpr int kInitialParameterVersion = 1;

enum class Kind
{
    Float,
    Choice,
    Bool
};

enum class Unit
{
    None,
    Percent,
    Milliseconds,
    Seconds,
    Hertz,
    Decibels
};

// ---------------------------------------------------------------------------
// Choice lists. Order is part of the saved state: append only.
// ---------------------------------------------------------------------------

enum class Routing
{
    Parallel = 0,
    DelayToReverb,
    ReverbToDelay
};
inline constexpr std::array<const char*, 3> kRoutingNames { "Parallel", "Delay > Reverb", "Reverb > Delay" };

enum class DelayMode
{
    Digital = 0,
    Tape,
    BucketBrigade,
    PingPong
};
inline constexpr std::array<const char*, 4> kDelayModeNames { "Digital", "Tape", "BBD", "Ping Pong" };

enum class ReverbMode
{
    Room = 0,
    Hall,
    Plate,
    Ambient,
    Shimmer
};
inline constexpr std::array<const char*, 5> kReverbModeNames { "Room", "Hall", "Plate", "Ambient", "Shimmer" };

/// Musical note division used when the delay is tempo-synced.
/// `quarterNotes` is the duration in quarter notes (beats in 4/4).
struct TempoDivision
{
    const char* name;
    double quarterNotes;
};

/// Sorted by duration so that sweeping the control is monotonic.
inline constexpr std::array<TempoDivision, 18> kTempoDivisions { {
    { "1/32T", 1.0 / 12.0 },
    { "1/32", 1.0 / 8.0 },
    { "1/16T", 1.0 / 6.0 },
    { "1/32D", 3.0 / 16.0 },
    { "1/16", 1.0 / 4.0 },
    { "1/8T", 1.0 / 3.0 },
    { "1/16D", 3.0 / 8.0 },
    { "1/8", 1.0 / 2.0 },
    { "1/4T", 2.0 / 3.0 },
    { "1/8D", 3.0 / 4.0 },
    { "1/4", 1.0 },
    { "1/2T", 4.0 / 3.0 },
    { "1/4D", 3.0 / 2.0 },
    { "1/2", 2.0 },
    { "1/1T", 8.0 / 3.0 },
    { "1/2D", 3.0 },
    { "1/1", 4.0 },
    { "1/1D", 6.0 },
} };

/// Pitch interval applied inside the Shimmer reverb feedback path.
struct ShimmerInterval
{
    const char* name;
    float semitones;
};

inline constexpr std::array<ShimmerInterval, 5> kShimmerIntervals { {
    { "-1 Oct", -12.0f },
    { "+5th", 7.0f },
    { "+1 Oct", 12.0f },
    { "+1 Oct +5th", 19.0f },
    { "+2 Oct", 24.0f },
} };

namespace detail
{
    template <typename T, std::size_t N>
    constexpr std::array<const char*, N> namesOf (const std::array<T, N>& items) noexcept
    {
        std::array<const char*, N> names {};
        for (std::size_t i = 0; i < N; ++i)
            names[i] = items[i].name;
        return names;
    }
} // namespace detail

inline constexpr auto kTempoDivisionNames = detail::namesOf (kTempoDivisions);
inline constexpr auto kShimmerIntervalNames = detail::namesOf (kShimmerIntervals);

/// Default tempo division index ("1/4").
inline constexpr int kDefaultTempoDivision = 10;
/// Default shimmer interval index ("+1 Oct").
inline constexpr int kDefaultShimmerInterval = 2;

// ---------------------------------------------------------------------------
// Parameters
// ---------------------------------------------------------------------------

enum class Param : std::size_t
{
    // Global
    routing,
    inputGain,
    outputGain,
    macroSpace,
    macroMovement,
    macroInfinite,

    // Delay engine
    delayBypass,
    delayMode,
    delayTime,
    delaySync,
    delayDivision,
    delayFeedback,
    delayTone,
    delayMix,
    delayModRate,
    delayModDepth,
    delayWidth,

    // Reverb engine
    reverbBypass,
    reverbMode,
    reverbDecay,
    reverbSize,
    reverbPreDelay,
    reverbDamping,
    reverbDiffusion,
    reverbModRate,
    reverbModDepth,
    reverbWidth,
    reverbFreeze,
    reverbShimmerInterval,
    reverbShimmerAmount,
    reverbMix,

    count
};

inline constexpr std::size_t kNumParams = static_cast<std::size_t> (Param::count);

constexpr std::size_t indexOf (Param p) noexcept { return static_cast<std::size_t> (p); }

struct ParamSpec
{
    Param param;
    const char* id;    ///< Stable host-facing identifier. Never change after release.
    const char* name;  ///< Display name.
    Kind kind;
    Unit unit;
    float minValue;
    float maxValue;
    float defaultValue;
    float skewCentre;  ///< Value placed at the control's mid-point; 0 = linear.
    float step;        ///< Quantisation interval; 0 = continuous.
    const char* const* choiceNames;
    std::size_t numChoices;
};

namespace detail
{
    constexpr ParamSpec makeFloat (Param p, const char* id, const char* name, Unit unit,
                                   float lo, float hi, float def, float skewCentre = 0.0f, float step = 0.0f) noexcept
    {
        return { p, id, name, Kind::Float, unit, lo, hi, def, skewCentre, step, nullptr, 0 };
    }

    constexpr ParamSpec makeBool (Param p, const char* id, const char* name, bool def) noexcept
    {
        return { p, id, name, Kind::Bool, Unit::None, 0.0f, 1.0f, def ? 1.0f : 0.0f, 0.0f, 1.0f, nullptr, 0 };
    }

    template <std::size_t N>
    constexpr ParamSpec makeChoice (Param p, const char* id, const char* name,
                                    const std::array<const char*, N>& names, int defaultIndex) noexcept
    {
        return { p, id, name, Kind::Choice, Unit::None, 0.0f, static_cast<float> (N - 1),
                 static_cast<float> (defaultIndex), 0.0f, 1.0f, names.data(), N };
    }
} // namespace detail

// clang-format off
inline constexpr std::array<ParamSpec, kNumParams> kParamSpecs { {
    // ---- Global ------------------------------------------------------------------------------------------
    detail::makeChoice (Param::routing,        "routing",        "Routing",        kRoutingNames, static_cast<int> (Routing::DelayToReverb)),
    detail::makeFloat  (Param::inputGain,      "inputGain",      "Input Gain",     Unit::Decibels, -24.0f, 24.0f, 0.0f),
    detail::makeFloat  (Param::outputGain,     "outputGain",     "Output Gain",    Unit::Decibels, -48.0f, 12.0f, 0.0f),
    detail::makeFloat  (Param::macroSpace,     "macroSpace",     "Space",          Unit::Percent,    0.0f, 100.0f, 0.0f),
    detail::makeFloat  (Param::macroMovement,  "macroMovement",  "Movement",       Unit::Percent,    0.0f, 100.0f, 0.0f),
    detail::makeFloat  (Param::macroInfinite,  "macroInfinite",  "Infinite",       Unit::Percent,    0.0f, 100.0f, 0.0f),

    // ---- Delay -------------------------------------------------------------------------------------------
    detail::makeBool   (Param::delayBypass,    "delayBypass",    "Delay Bypass",   false),
    detail::makeChoice (Param::delayMode,      "delayMode",      "Delay Mode",     kDelayModeNames, static_cast<int> (DelayMode::Digital)),
    detail::makeFloat  (Param::delayTime,      "delayTime",      "Delay Time",     Unit::Milliseconds, 1.0f, 3000.0f, 375.0f, 400.0f),
    detail::makeBool   (Param::delaySync,      "delaySync",      "Delay Sync",     false),
    detail::makeChoice (Param::delayDivision,  "delayDivision",  "Delay Division", kTempoDivisionNames, kDefaultTempoDivision),
    detail::makeFloat  (Param::delayFeedback,  "delayFeedback",  "Feedback",       Unit::Percent,    0.0f, 100.0f, 35.0f),
    detail::makeFloat  (Param::delayTone,      "delayTone",      "Delay Tone",     Unit::Percent, -100.0f, 100.0f, 0.0f),
    detail::makeFloat  (Param::delayMix,       "delayMix",       "Delay Mix",      Unit::Percent,    0.0f, 100.0f, 30.0f),
    detail::makeFloat  (Param::delayModRate,   "delayModRate",   "Delay Mod Rate", Unit::Hertz,      0.05f, 10.0f, 0.5f, 1.0f),
    detail::makeFloat  (Param::delayModDepth,  "delayModDepth",  "Delay Mod Depth",Unit::Percent,    0.0f, 100.0f, 10.0f),
    detail::makeFloat  (Param::delayWidth,     "delayWidth",     "Delay Width",    Unit::Percent,    0.0f, 200.0f, 100.0f),

    // ---- Reverb ------------------------------------------------------------------------------------------
    detail::makeBool   (Param::reverbBypass,   "reverbBypass",   "Reverb Bypass",  false),
    detail::makeChoice (Param::reverbMode,     "reverbMode",     "Reverb Mode",    kReverbModeNames, static_cast<int> (ReverbMode::Hall)),
    detail::makeFloat  (Param::reverbDecay,    "reverbDecay",    "Decay (RT60)",   Unit::Seconds,    0.1f, 30.0f, 2.5f, 2.5f),
    detail::makeFloat  (Param::reverbSize,     "reverbSize",     "Size",           Unit::Percent,    0.0f, 100.0f, 50.0f),
    detail::makeFloat  (Param::reverbPreDelay, "reverbPreDelay", "Pre-Delay",      Unit::Milliseconds, 0.0f, 250.0f, 10.0f, 50.0f),
    detail::makeFloat  (Param::reverbDamping,  "reverbDamping",  "Damping",        Unit::Percent,    0.0f, 100.0f, 40.0f),
    detail::makeFloat  (Param::reverbDiffusion,"reverbDiffusion","Diffusion",      Unit::Percent,    0.0f, 100.0f, 70.0f),
    detail::makeFloat  (Param::reverbModRate,  "reverbModRate",  "Reverb Mod Rate",Unit::Hertz,      0.05f, 5.0f, 0.4f, 1.0f),
    detail::makeFloat  (Param::reverbModDepth, "reverbModDepth", "Reverb Mod Depth",Unit::Percent,   0.0f, 100.0f, 20.0f),
    detail::makeFloat  (Param::reverbWidth,    "reverbWidth",    "Reverb Width",   Unit::Percent,    0.0f, 200.0f, 100.0f),
    detail::makeBool   (Param::reverbFreeze,   "reverbFreeze",   "Freeze",         false),
    detail::makeChoice (Param::reverbShimmerInterval, "reverbShimmerInterval", "Shimmer Interval", kShimmerIntervalNames, kDefaultShimmerInterval),
    detail::makeFloat  (Param::reverbShimmerAmount,   "reverbShimmerAmount",   "Shimmer Amount",   Unit::Percent, 0.0f, 100.0f, 50.0f),
    detail::makeFloat  (Param::reverbMix,      "reverbMix",      "Reverb Mix",     Unit::Percent,    0.0f, 100.0f, 25.0f),
} };
// clang-format on

constexpr const ParamSpec& spec (Param p) noexcept { return kParamSpecs[indexOf (p)]; }

// ---------------------------------------------------------------------------
// Compile-time validation of the table.
// ---------------------------------------------------------------------------
namespace detail
{
    constexpr bool equalStrings (const char* a, const char* b) noexcept
    {
        while (*a != '\0' && *a == *b)
        {
            ++a;
            ++b;
        }
        return *a == *b;
    }

    constexpr bool specIsValid (const ParamSpec& s, std::size_t position) noexcept
    {
        if (indexOf (s.param) != position)
            return false;
        if (s.id == nullptr || s.id[0] == '\0' || s.name == nullptr)
            return false;
        if (! (s.minValue < s.maxValue))
            return false;
        if (s.defaultValue < s.minValue || s.defaultValue > s.maxValue)
            return false;
        if (s.skewCentre < 0.0f)
            return false;
        if (s.skewCentre > 0.0f && (s.skewCentre <= s.minValue || s.skewCentre >= s.maxValue))
            return false;
        if (s.kind == Kind::Choice && (s.choiceNames == nullptr || s.numChoices < 2))
            return false;
        if (s.kind != Kind::Choice && s.choiceNames != nullptr)
            return false;
        return true;
    }

    constexpr bool allSpecsValid() noexcept
    {
        for (std::size_t i = 0; i < kNumParams; ++i)
        {
            if (! specIsValid (kParamSpecs[i], i))
                return false;

            for (std::size_t j = i + 1; j < kNumParams; ++j)
                if (equalStrings (kParamSpecs[i].id, kParamSpecs[j].id))
                    return false;
        }
        return true;
    }

    constexpr bool tempoDivisionsAscending() noexcept
    {
        for (std::size_t i = 1; i < kTempoDivisions.size(); ++i)
            if (! (kTempoDivisions[i - 1].quarterNotes < kTempoDivisions[i].quarterNotes))
                return false;
        return true;
    }
} // namespace detail

static_assert (detail::allSpecsValid(), "Parameter spec table is inconsistent (order, ranges, defaults or duplicate IDs)");
static_assert (detail::tempoDivisionsAscending(), "Tempo divisions must be sorted by duration");
static_assert (detail::equalStrings (kTempoDivisionNames[kDefaultTempoDivision], "1/4"), "Default division must be 1/4");

} // namespace echoform::params
