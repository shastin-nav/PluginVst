#include <juce_events/juce_events.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <gtest/gtest.h>

// JUCE needs its message manager (and GUI subsystem for the editor tests)
// initialised for the lifetime of the test run.
int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;
    ::testing::InitGoogleTest (&argc, argv);
    return RUN_ALL_TESTS();
}
