#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace echoform
{

class EchoformAudioProcessor;

/// Interim editor (milestone P1): brand frame plus an amber status lamp per
/// engine (lit = active, dim = bypassed). The custom control set is milestone
/// P2; until then parameters are reachable through the host's automation and
/// generic parameter views.
///
/// The editor talks to the processor only through its parameter state, on the
/// message thread; it never touches DSP objects.
class EchoformAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                           private juce::Timer
{
public:
    explicit EchoformAudioProcessorEditor (EchoformAudioProcessor& owner);
    ~EchoformAudioProcessorEditor() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

    bool isDelayLampLit() const noexcept { return delayActive; }
    bool isReverbLampLit() const noexcept { return reverbActive; }

    static constexpr int kWidth = 900;
    static constexpr int kHeight = 540;

private:
    void timerCallback() override;
    void refreshEngineStatus();

    const std::atomic<float>* delayBypass = nullptr;
    const std::atomic<float>* reverbBypass = nullptr;
    bool delayActive = true;
    bool reverbActive = true;

    juce::Rectangle<int> delayArea, reverbArea, headerArea, footerArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EchoformAudioProcessorEditor)
};

} // namespace echoform
