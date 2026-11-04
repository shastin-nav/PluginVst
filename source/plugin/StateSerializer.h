#pragma once

// Plug-in state (host sessions, presets) <-> AudioProcessorValueTreeState.
//
// Format: JUCE binary-wrapped XML whose root tag is kStateType, carrying a
// schema version for future migrations and one PARAM child per parameter.
//
// Loading is deterministic: the resulting state depends only on the blob,
// never on the values that happened to be live before loading. Parameters
// missing from the blob (e.g. added in a later version) take their defaults;
// unknown, non-finite or out-of-range values are rejected or clamped.

#include <juce_audio_processors/juce_audio_processors.h>

namespace echoform::state
{

inline constexpr int kSchemaVersion = 1;
inline constexpr const char* kStateType = "ECHOFORM_STATE";
inline constexpr const char* kSchemaVersionProperty = "schemaVersion";
inline constexpr const char* kPluginVersionProperty = "pluginVersion";

/// Serialises the current parameter state.
void write (juce::AudioProcessorValueTreeState& state, const juce::String& pluginVersion, juce::MemoryBlock& destination);

/// Replaces the parameter state from a blob produced by write().
/// @returns false (leaving the state untouched) if the blob is not a valid ECHOFORM state.
bool read (juce::AudioProcessorValueTreeState& state, const void* data, int sizeInBytes);

/// Builds a complete, validated parameter tree from a possibly partial one.
juce::ValueTree buildCompleteState (const juce::ValueTree& loaded);

} // namespace echoform::state
