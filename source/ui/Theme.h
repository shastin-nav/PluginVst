#pragma once

// ECHOFORM visual identity. All UI colours come from here; components never
// hard-code colour values.

#include <juce_graphics/juce_graphics.h>

namespace echoform::ui
{

struct Palette
{
    // Dark charcoal surfaces
    static constexpr juce::uint32 background = 0xff161514;
    static constexpr juce::uint32 surface = 0xff201e1c;
    static constexpr juce::uint32 surfaceRaised = 0xff2a2724;
    static constexpr juce::uint32 outline = 0xff3a3631;

    // Warm ivory text and markings
    static constexpr juce::uint32 ivory = 0xffede4d0;
    static constexpr juce::uint32 ivoryMuted = 0xffa9a08f;

    // Muted copper accents
    static constexpr juce::uint32 copper = 0xffb07650;
    static constexpr juce::uint32 copperDark = 0xff7a4f35;

    // Amber indicators
    static constexpr juce::uint32 amber = 0xfff0a830;
    static constexpr juce::uint32 amberDim = 0xff5c4214;
};

inline juce::Colour colour (juce::uint32 argb) { return juce::Colour (argb); }

} // namespace echoform::ui
