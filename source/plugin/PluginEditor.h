#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace echoform
{

class EchoformAudioProcessor;

/// Milestone P0 editor: brand frame only. It deliberately exposes no controls
/// until the engines they drive exist (every control must work). The custom
/// control set is milestone P2.
///
/// The editor talks to the processor only through its parameter state; it
/// never touches DSP objects.
class EchoformAudioProcessorEditor final : public juce::AudioProcessorEditor
{
public:
    explicit EchoformAudioProcessorEditor (EchoformAudioProcessor& owner);
    ~EchoformAudioProcessorEditor() override = default;

    void paint (juce::Graphics& g) override;
    void resized() override;

    static constexpr int kWidth = 900;
    static constexpr int kHeight = 540;

private:
    juce::Rectangle<int> delayArea, reverbArea, headerArea, footerArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EchoformAudioProcessorEditor)
};

} // namespace echoform
