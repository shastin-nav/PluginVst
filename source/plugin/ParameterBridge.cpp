#include "plugin/ParameterBridge.h"

#include <stdexcept>
#include <string>

namespace echoform
{

ParameterBridge::ParameterBridge (juce::AudioProcessorValueTreeState& state)
{
    for (std::size_t i = 0; i < params::kNumParams; ++i)
    {
        sources[i] = state.getRawParameterValue (params::kParamSpecs[i].id);

        // Every spec must have a matching host parameter. A null here means the
        // layout and the spec table have diverged, which is a programming error.
        jassert (sources[i] != nullptr);
        if (sources[i] == nullptr)
            throw std::logic_error (std::string ("Missing parameter: ") + params::kParamSpecs[i].id);
    }
}

void ParameterBridge::update (params::ParameterSnapshot& snapshot) const noexcept
{
    for (std::size_t i = 0; i < params::kNumParams; ++i)
        snapshot.set (static_cast<params::Param> (i), sources[i]->load (std::memory_order_relaxed));
}

} // namespace echoform
