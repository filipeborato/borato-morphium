#include <catch2/catch_session.hpp>
#include <juce_events/juce_events.h>

// Headless JUCE bootstrap: MorphiumAudioProcessor needs a live MessageManager
// (APVTS, parameter notifications), but no window is ever opened, so this runs
// fine on CI without a display (Ubuntu still wants Xvfb for the X11 connection).
int main (int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    return Catch::Session().run (argc, argv);
}
