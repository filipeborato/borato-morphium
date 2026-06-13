#include <catch2/catch_test_macros.hpp>

#include "SpectralUtils.h"
#include "PluginProcessor.h"
#include "Presets.h"

using namespace morphium;
using namespace morphium::test;

// The broadest regression net: every factory preset is rendered end to end and
// dumped as a WAV artifact (tests/artifacts/) for human A/B listening between
// stages of the DSP redesign.
TEST_CASE ("Presets: every factory preset renders clean and decays", "[presets][smoke]")
{
    constexpr int sr = 44100;
    const auto& presets = getFactoryPresets();
    REQUIRE (! presets.empty());

    for (int index = 0; index < (int) presets.size(); ++index)
    {
        MorphiumAudioProcessor processor;
        processor.setCurrentProgram (index);
        processor.setDeterministicSeedForTests (12345);

        // 2 s note + 6 s post-roll: longest release is 3 s and large reverb
        // tails (CINEMATIC SCRAPE has size 1.0) need room to die out.
        const auto buffer = renderSingleNote (processor, 69, 0.8f, 2 * sr, 8 * sr);

        const juce::String name (presets[(size_t) index].name);
        INFO ("preset " << index << " (" << name << ")");

        REQUIRE (! containsNonFinite (buffer));
        // The master safety clip bounds the output at 1.0.
        CHECK (peakLinear (buffer) < 1.01f);

        // Stuck-note criterion: either the tail is effectively silent, or it
        // has decayed well below the level right after note-off (huge reverbs
        // like CINEMATIC SCRAPE at size 1.0 legitimately ring past 6 s).
        const float tailDb = rmsDb (buffer, buffer.getNumSamples() - sr / 2, sr / 2);
        const float afterReleaseDb = rmsDb (buffer, 2 * sr, sr / 2);
        INFO ("tail RMS = " << tailDb << " dBFS, just after note-off = "
              << afterReleaseDb << " dBFS");
        CHECK ((tailDb < -40.0f || tailDb < afterReleaseDb - 20.0f));

        // Must actually make sound.
        CHECK (rmsDb (buffer, 0, 2 * sr) > -60.0f);

        writeWavArtifact (buffer, sr, "preset_" + juce::String (index) + "_"
                          + name.toLowerCase().replaceCharacters (" ", "_"));
    }
}
