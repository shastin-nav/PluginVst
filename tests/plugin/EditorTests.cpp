#include "plugin/TestHelpers.h"

#include "plugin/PluginEditor.h"
#include "ui/Theme.h"

#include <gtest/gtest.h>

using namespace echoform;

namespace
{
/// Owns an editor the way a plug-in wrapper does: JUCE requires
/// editorBeingDeleted() to be called on the processor before the editor dies.
struct HostedEditor
{
    explicit HostedEditor (juce::AudioProcessor& p) : processor (p), editor (p.createEditorAndMakeActive()) {}

    ~HostedEditor()
    {
        if (editor != nullptr)
            processor.editorBeingDeleted (editor.get());
    }

    HostedEditor (const HostedEditor&) = delete;
    HostedEditor& operator= (const HostedEditor&) = delete;

    juce::AudioProcessor& processor;
    std::unique_ptr<juce::AudioProcessorEditor> editor;
};
} // namespace

TEST (Editor, OpensWithFixedSizeAndPaintsTheme)
{
    EchoformAudioProcessor processor;
    ASSERT_TRUE (processor.hasEditor());

    auto hosted = std::make_unique<HostedEditor> (processor);
    auto& editor = hosted->editor;
    ASSERT_NE (editor, nullptr);
    EXPECT_EQ (processor.getActiveEditor(), editor.get());
    EXPECT_EQ (editor->getWidth(), EchoformAudioProcessorEditor::kWidth);
    EXPECT_EQ (editor->getHeight(), EchoformAudioProcessorEditor::kHeight);
    EXPECT_TRUE (editor->isOpaque());

    // Render off-screen: no display needed.
    const auto image = editor->createComponentSnapshot (editor->getLocalBounds(), true, 1.0f);
    ASSERT_TRUE (image.isValid());
    EXPECT_EQ (image.getPixelAt (2, 2).getARGB(), ui::Palette::background);

    hosted.reset();
    EXPECT_EQ (processor.getActiveEditor(), nullptr);
}

TEST (Editor, CanBeOpenedAndClosedRepeatedlyWhileProcessing)
{
    auto processor = test::makePreparedProcessor();
    juce::AudioBuffer<float> buffer (2, 256);
    juce::MidiBuffer midi;

    for (int i = 0; i < 10; ++i)
    {
        HostedEditor hosted { *processor };
        ASSERT_NE (hosted.editor, nullptr);
        test::fillWithNoise (buffer, static_cast<unsigned> (i));
        processor->processBlock (buffer, midi);
    }
    EXPECT_EQ (processor->getActiveEditor(), nullptr);
}
