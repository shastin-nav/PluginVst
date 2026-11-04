#include "plugin/ParameterLayout.h"

#include "params/ParameterSpecs.h"

namespace echoform
{
namespace
{
    using params::Kind;
    using params::ParamSpec;
    using params::Unit;

    juce::NormalisableRange<float> makeRange (const ParamSpec& s)
    {
        juce::NormalisableRange<float> range { s.minValue, s.maxValue, s.step };
        if (s.skewCentre > 0.0f)
            range.setSkewForCentre (s.skewCentre);
        return range;
    }

    juce::String formatValue (const ParamSpec& s, float value)
    {
        switch (s.unit)
        {
            case Unit::Percent:
                return juce::String (juce::roundToInt (value)) + " %";
            case Unit::Milliseconds:
                if (value >= 1000.0f)
                    return juce::String (value / 1000.0f, 2) + " s";
                return juce::String (value, value < 10.0f ? 1 : 0) + " ms";
            case Unit::Seconds:
                return juce::String (value, value < 10.0f ? 2 : 1) + " s";
            case Unit::Hertz:
                return juce::String (value, 2) + " Hz";
            case Unit::Decibels:
                return (value > 0.0f ? "+" : "") + juce::String (value, 1) + " dB";
            case Unit::None:
                break;
        }
        return juce::String (value, 2);
    }

    /// Accepts the formats produced above ("1.20 s" -> 1200 ms, "35 %", "-3 dB").
    float parseValue (const ParamSpec& s, const juce::String& text)
    {
        const auto trimmed = text.trim();
        auto value = trimmed.getFloatValue();

        if (s.unit == Unit::Milliseconds && trimmed.endsWithIgnoreCase ("s") && ! trimmed.endsWithIgnoreCase ("ms"))
            value *= 1000.0f;

        return juce::jlimit (s.minValue, s.maxValue, value);
    }

    juce::String unitLabel (Unit unit)
    {
        switch (unit)
        {
            case Unit::Percent:      return "%";
            case Unit::Milliseconds: return "ms";
            case Unit::Seconds:      return "s";
            case Unit::Hertz:        return "Hz";
            case Unit::Decibels:     return "dB";
            case Unit::None:         break;
        }
        return {};
    }

    juce::StringArray choiceNames (const ParamSpec& s)
    {
        juce::StringArray names;
        for (std::size_t i = 0; i < s.numChoices; ++i)
            names.add (s.choiceNames[i]);
        return names;
    }
} // namespace

juce::String formatParameterValue (std::size_t specIndex, float value, int maximumStringLength)
{
    jassert (specIndex < params::kNumParams);
    const auto& s = params::kParamSpecs[specIndex];

    juce::String text;
    switch (s.kind)
    {
        case Kind::Bool:
            text = value >= 0.5f ? "On" : "Off";
            break;
        case Kind::Choice:
        {
            const auto index = juce::jlimit (0, static_cast<int> (s.numChoices) - 1, juce::roundToInt (value));
            text = s.choiceNames[index];
            break;
        }
        case Kind::Float:
            text = formatValue (s, value);
            break;
    }

    return maximumStringLength > 0 ? text.substring (0, maximumStringLength) : text;
}

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    for (std::size_t i = 0; i < params::kNumParams; ++i)
    {
        const auto& s = params::kParamSpecs[i];
        const juce::ParameterID id { s.id, params::kInitialParameterVersion };

        switch (s.kind)
        {
            case Kind::Float:
            {
                auto attributes = juce::AudioParameterFloatAttributes()
                                      .withLabel (unitLabel (s.unit))
                                      .withStringFromValueFunction ([i] (float v, int maxLen)
                                                                    { return formatParameterValue (i, v, maxLen); })
                                      .withValueFromStringFunction ([&s] (const juce::String& text)
                                                                    { return parseValue (s, text); });

                layout.add (std::make_unique<juce::AudioParameterFloat> (id, s.name, makeRange (s), s.defaultValue,
                                                                         std::move (attributes)));
                break;
            }
            case Kind::Choice:
                layout.add (std::make_unique<juce::AudioParameterChoice> (id, s.name, choiceNames (s),
                                                                          juce::roundToInt (s.defaultValue)));
                break;
            case Kind::Bool:
                layout.add (std::make_unique<juce::AudioParameterBool> (id, s.name, s.defaultValue >= 0.5f));
                break;
        }
    }

    return layout;
}

} // namespace echoform
