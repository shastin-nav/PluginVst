#include "plugin/PluginEditor.h"

#include "plugin/PluginProcessor.h"
#include "ui/Theme.h"

namespace echoform
{
namespace
{
    using ui::colour;
    using ui::Palette;

    void drawEnginePanel (juce::Graphics& g, juce::Rectangle<int> area, const juce::String& title, bool active)
    {
        const auto bounds = area.toFloat();

        g.setColour (colour (Palette::surface));
        g.fillRoundedRectangle (bounds, 6.0f);
        g.setColour (colour (Palette::outline));
        g.drawRoundedRectangle (bounds.reduced (0.5f), 6.0f, 1.0f);

        auto header = area.removeFromTop (44).reduced (18, 0);

        // Engine status lamp: lit amber = active, dim = bypassed.
        const auto lamp = header.removeFromRight (10).withSizeKeepingCentre (10, 10).toFloat();
        g.setColour (colour (active ? Palette::amber : Palette::amberDim));
        g.fillEllipse (lamp);

        g.setColour (colour (Palette::ivory));
        g.setFont (juce::FontOptions (15.0f, juce::Font::bold));
        g.drawText (title, header, juce::Justification::centredLeft);

        g.setColour (colour (Palette::copperDark));
        g.fillRect (area.removeFromTop (1).reduced (18, 0));

        g.setColour (colour (Palette::ivoryMuted));
        g.setFont (juce::FontOptions (13.0f));
        g.drawText (active ? "Active" : "Bypassed", area.removeFromTop (area.getHeight() / 2 + 10), juce::Justification::centredBottom);
        g.setFont (juce::FontOptions (12.0f));
        g.drawText ("Controls arrive in milestone P2", area.withTrimmedTop (6), juce::Justification::centredTop);
    }
} // namespace

EchoformAudioProcessorEditor::EchoformAudioProcessorEditor (EchoformAudioProcessor& owner)
    : juce::AudioProcessorEditor (owner),
      delayBypass (owner.getValueTreeState().getRawParameterValue ("delayBypass")),
      reverbBypass (owner.getValueTreeState().getRawParameterValue ("reverbBypass"))
{
    setOpaque (true);
    setSize (kWidth, kHeight);
    refreshEngineStatus();
    startTimerHz (10);
}

EchoformAudioProcessorEditor::~EchoformAudioProcessorEditor() { stopTimer(); }

void EchoformAudioProcessorEditor::refreshEngineStatus()
{
    const auto delayNow = delayBypass == nullptr || delayBypass->load() < 0.5f;
    const auto reverbNow = reverbBypass == nullptr || reverbBypass->load() < 0.5f;
    if (delayNow != delayActive || reverbNow != reverbActive)
    {
        delayActive = delayNow;
        reverbActive = reverbNow;
        repaint();
    }
}

void EchoformAudioProcessorEditor::timerCallback() { refreshEngineStatus(); }

void EchoformAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (colour (Palette::background));

    // Header: wordmark + copper rule.
    {
        auto area = headerArea.reduced (28, 0);

        g.setColour (colour (Palette::ivory));
        g.setFont (juce::FontOptions (30.0f, juce::Font::bold));
        g.drawText ("ECHOFORM", area.removeFromLeft (260), juce::Justification::centredLeft);

        g.setColour (colour (Palette::ivoryMuted));
        g.setFont (juce::FontOptions (13.0f));
        g.drawText ("STEREO DELAY  /  REVERB", area, juce::Justification::centredRight);

        g.setColour (colour (Palette::copper));
        g.fillRect (headerArea.reduced (28, 0).removeFromBottom (2));
    }

    drawEnginePanel (g, delayArea, "DELAY", delayActive);
    drawEnginePanel (g, reverbArea, "REVERB", reverbActive);

    // Footer: build identification.
    g.setColour (colour (Palette::ivoryMuted));
    g.setFont (juce::FontOptions (12.0f));
    g.drawText (juce::String ("v") + JucePlugin_VersionString + "  -  core DSP build (P1)",
                footerArea.reduced (28, 0), juce::Justification::centredLeft);
}

void EchoformAudioProcessorEditor::resized()
{
    auto area = getLocalBounds();
    headerArea = area.removeFromTop (78);
    footerArea = area.removeFromBottom (40);

    area = area.reduced (28, 16);
    const auto gap = 20;
    delayArea = area.removeFromLeft ((area.getWidth() - gap) / 2);
    area.removeFromLeft (gap);
    reverbArea = area;
}

} // namespace echoform
