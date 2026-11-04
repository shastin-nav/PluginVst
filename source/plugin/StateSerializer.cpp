#include "plugin/StateSerializer.h"

#include "params/ParameterSpecs.h"

#include <cmath>

namespace echoform::state
{
namespace
{
    // Property names used internally by juce::AudioProcessorValueTreeState.
    const juce::Identifier paramType { "PARAM" };
    const juce::Identifier idProperty { "id" };
    const juce::Identifier valueProperty { "value" };

    float validatedValue (const params::ParamSpec& s, const juce::var& stored)
    {
        if (! (stored.isDouble() || stored.isInt() || stored.isInt64() || stored.isBool() || stored.isString()))
            return s.defaultValue;

        const auto value = static_cast<double> (stored);
        if (! std::isfinite (value))
            return s.defaultValue;

        auto result = juce::jlimit (s.minValue, s.maxValue, static_cast<float> (value));

        if (s.kind != params::Kind::Float)
            result = std::round (result);

        return result;
    }
} // namespace

juce::ValueTree buildCompleteState (const juce::ValueTree& loaded)
{
    juce::ValueTree complete { juce::Identifier (kStateType) };
    complete.setProperty (kSchemaVersionProperty, kSchemaVersion, nullptr);

    for (const auto& s : params::kParamSpecs)
    {
        float value = s.defaultValue;

        const auto stored = loaded.getChildWithProperty (idProperty, juce::String (s.id));
        if (stored.isValid() && stored.hasType (paramType) && stored.hasProperty (valueProperty))
            value = validatedValue (s, stored.getProperty (valueProperty));

        juce::ValueTree child { paramType };
        child.setProperty (idProperty, juce::String (s.id), nullptr);
        child.setProperty (valueProperty, static_cast<double> (value), nullptr);
        complete.appendChild (child, nullptr);
    }

    return complete;
}

void write (juce::AudioProcessorValueTreeState& state, const juce::String& pluginVersion, juce::MemoryBlock& destination)
{
    auto tree = state.copyState();
    tree.setProperty (kSchemaVersionProperty, kSchemaVersion, nullptr);
    tree.setProperty (kPluginVersionProperty, pluginVersion, nullptr);

    if (auto xml = tree.createXml())
        juce::AudioProcessor::copyXmlToBinary (*xml, destination);
}

bool read (juce::AudioProcessorValueTreeState& state, const void* data, int sizeInBytes)
{
    if (data == nullptr || sizeInBytes <= 0)
        return false;

    const auto xml = juce::AudioProcessor::getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr || ! xml->hasTagName (kStateType))
        return false;

    const auto loaded = juce::ValueTree::fromXml (*xml);
    if (! loaded.isValid())
        return false;

    // Schema migrations go here when kSchemaVersion is bumped. A blob from a
    // newer schema is loaded best-effort: known parameters are restored.
    state.replaceState (buildCompleteState (loaded));
    return true;
}

} // namespace echoform::state
