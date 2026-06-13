#include <catch2/catch_test_macros.hpp>

#include "SpectralUtils.h"
#include "PluginProcessor.h"

using namespace morphium;
using namespace morphium::test;

TEST_CASE ("Processor: clean render across sample rates", "[integration][samplerate]")
{
    for (double sampleRate : { 44100.0, 48000.0, 96000.0 })
    {
        MorphiumAudioProcessor processor;
        processor.setDeterministicSeedForTests (12345);

        const int sr = (int) sampleRate;
        const auto buffer = renderSingleNote (processor, 69, 0.8f, sr, sr, sampleRate);

        INFO ("sample rate " << sampleRate);
        REQUIRE (! containsNonFinite (buffer));
        CHECK (rmsDb (buffer, 0, sr) > -60.0f);

        // Default excitation is STRIKE (decaying sine at f0): the note must
        // sit at 440 Hz regardless of the engine sample rate.
        const auto spectrum = analyseBuffer (buffer, 0, 0, 16384, sampleRate);
        CHECK (hasPeakNear (spectrum, 440.0f, 25.0f, -30.0f));
    }
}

TEST_CASE ("Processor: clean render across block sizes", "[integration][blocksize]")
{
    constexpr int sr = 44100;

    for (int blockSize : { 16, 64, 128, 256, 512, 1024, 333, 777 })
    {
        MorphiumAudioProcessor processor;
        processor.setDeterministicSeedForTests (12345);

        const auto buffer = renderSingleNote (processor, 69, 0.8f, sr, sr,
                                              44100.0, blockSize);

        INFO ("block size " << blockSize);
        REQUIRE (! containsNonFinite (buffer));
        CHECK (rmsDb (buffer, 0, sr) > -60.0f);
    }
}

TEST_CASE ("Processor: waveguide presets play in tune and decay", "[integration][waveguide]")
{
    constexpr int sr = 44100;

    // CONCRETE PIANO (program 18) is a pure waveguide strike patch.
    MorphiumAudioProcessor processor;
    processor.setCurrentProgram (18);
    processor.setDeterministicSeedForTests (12345);

    const auto buffer = renderSingleNote (processor, 69, 0.9f, sr, 6 * sr);
    REQUIRE (! containsNonFinite (buffer));
    CHECK (peakLinear (buffer) < 1.01f);
    CHECK (rmsDb (buffer, 0, sr) > -60.0f);

    // The string must ring at the played note.
    const auto spectrum = analyseBuffer (buffer, 0, sr / 10, 16384, 44100.0);
    CHECK (hasPeakNear (spectrum, 440.0f, 30.0f, -30.0f));

    // And the note must die out (no immortal string).
    const float tailDb = rmsDb (buffer, buffer.getNumSamples() - sr / 2, sr / 2);
    INFO ("tail RMS = " << tailDb << " dBFS");
    CHECK (tailDb < -40.0f);
}

// Spectral golden files: a seeded render's strongest peaks are snapshotted to
// tests/golden/*.json. Any DSP change that moves them must be deliberate --
// regenerate with the MORPHIUM_GENERATE_GOLDEN environment variable set.
TEST_CASE ("Processor: golden spectra for signature presets", "[integration][golden]")
{
    constexpr int sr = 44100;

    struct GoldenCase { int program; const char* name; };
    const GoldenCase cases[] = {
        { 1, "glass_cold_room" },
        { 3, "wooden_marimba" },
    };

    for (const auto& goldenCase : cases)
    {
        MorphiumAudioProcessor processor;
        processor.setCurrentProgram (goldenCase.program);
        processor.setDeterministicSeedForTests (12345);

        const auto buffer = renderSingleNote (processor, 69, 0.8f, 2 * sr, 2 * sr);
        REQUIRE (! containsNonFinite (buffer));

        // Analyse just past the attack; keep only strong peaks so the
        // comparison is robust to platform float differences.
        const auto spectrum = analyseBuffer (buffer, 0, sr / 10, 16384, 44100.0);
        const auto peaks = findPeaks (spectrum, -30.0f, 50.0f, 8000.0f);

        juce::String details;
        const bool matches = matchesGolden (peaks, goldenCase.name, 5.0f, 4.0f, &details);

        INFO ("golden \"" << goldenCase.name << "\": " << details.toStdString());
        CHECK (matches);
    }
}
