#include <catch2/catch_test_macros.hpp>

#include "SpectralUtils.h"
#include "Core/ExcitationCore.h"

using namespace morphium;
using namespace morphium::test;

namespace
{
    // Renders one excitation source in isolation (no matter, no envelope).
    std::vector<float> renderExcitation (ExcitationType type, float frequency,
                                         int numSamples, double sampleRate = 44100.0,
                                         juce::int64 seed = 12345)
    {
        ExcitationCore core;
        core.prepare (sampleRate);
        core.setSeed (seed);
        core.setType (type);
        core.setFrequency (frequency);
        core.trigger();

        std::vector<float> out ((size_t) numSamples);
        for (auto& sample : out)
            sample = core.processSample();
        return out;
    }

    Spectrum analyseTail (const std::vector<float>& samples, double sampleRate = 44100.0)
    {
        // Skip the attack transient so steady-state spectra are clean.
        const int n = juce::jmin ((int) samples.size(), 16384);
        const int start = (int) samples.size() - n;
        return analyse (samples.data() + start, n, sampleRate);
    }
}

TEST_CASE ("ExcitationCore: every source renders finite, bounded output", "[excitation][safety]")
{
    for (int typeIndex = 0; typeIndex < (int) ExcitationType::NumTypes; ++typeIndex)
    {
        const auto samples = renderExcitation ((ExcitationType) typeIndex, 440.0f, 8192);

        float peak = 0.0f;
        bool finite = true;
        for (float s : samples)
        {
            finite = finite && std::isfinite (s);
            peak = juce::jmax (peak, std::abs (s));
        }

        INFO ("excitation type " << typeIndex << " ("
              << getExcitationTypeNames()[typeIndex].toStdString() << ")");
        CHECK (finite);
        CHECK (peak < 4.0f);
    }
}

TEST_CASE ("ExcitationCore: Bow fundamental lands on the note", "[excitation][tonal]")
{
    const auto samples = renderExcitation (ExcitationType::Bow, 440.0f, 32768);
    const auto spectrum = analyseTail (samples);

    const float f0 = detectF0 (spectrum, 300.0f, 600.0f);
    INFO ("detected f0 = " << f0 << " Hz");
    // Bow carries a deliberate +-0.4% random pitch drift, so the tolerance is
    // wider than the FFT accuracy: +-15 cents.
    CHECK (std::abs (1200.0f * std::log2 (f0 / 440.0f)) < 15.0f);
}

TEST_CASE ("ExcitationCore: pitched sources have a partial at the note", "[excitation][tonal]")
{
    struct Expectation { ExcitationType type; float tolCents; float minDbc; };
    const Expectation expectations[] = {
        { ExcitationType::Strike, 15.0f, -25.0f },
        { ExcitationType::Tape,   40.0f, -25.0f },   // wow modulates pitch +-1%
        { ExcitationType::Voice,  15.0f, -35.0f },   // formants outweigh the fundamental
    };

    for (const auto& expectation : expectations)
    {
        const auto samples = renderExcitation (expectation.type, 440.0f, 32768);
        const auto spectrum = analyseTail (samples);

        INFO ("excitation type " << (int) expectation.type);
        CHECK (hasPeakNear (spectrum, 440.0f, expectation.tolCents, expectation.minDbc));
    }
}

TEST_CASE ("ExcitationCore: Noise source is band-shaped around 2*f0", "[excitation][tonal]")
{
    const auto samples = renderExcitation (ExcitationType::Noise, 440.0f, 32768);
    const auto spectrum = analyseTail (samples);

    const float centroid = spectralCentroidHz (spectrum);
    INFO ("noise centroid = " << centroid << " Hz (band-pass tuned to 880 Hz)");
    // The Q=1.5 band-pass has shallow skirts, so the linear-frequency centroid
    // sits well above the 880 Hz centre (measured ~4 kHz); unfiltered white
    // noise would sit near 11 kHz.
    CHECK (centroid > 500.0f);
    CHECK (centroid < 5500.0f);
}

// The aliasing regression test. The naive saw (Tape) and naive pulse (Voice)
// fail it at the baseline; PolyBLEP (Stage 1) must bring them under the
// threshold. Thresholds were calibrated against measured baseline values --
// see comments below.
TEST_CASE ("ExcitationCore: aliasing stays below threshold at F6", "[excitation][aliasing]")
{
    const float f6 = 1396.91f; // MIDI 89

    SECTION ("Tape (saw)")
    {
        const auto samples = renderExcitation (ExcitationType::Tape, f6, 49152);
        const auto spectrum = analyseTail (samples);

        // +-2.5% harmonic window: wow/flutter legitimately smear the partials.
        const float ratio = nonHarmonicRatioDb (spectrum, f6, 0.025f);
        INFO ("Tape non-harmonic energy = " << ratio << " dB vs harmonics");
        CHECK (ratio < -30.0f);
    }

    SECTION ("Voice (pulse)")
    {
        const auto samples = renderExcitation (ExcitationType::Voice, f6, 49152);
        const auto spectrum = analyseTail (samples);

        const float ratio = nonHarmonicRatioDb (spectrum, f6, 0.008f);
        INFO ("Voice non-harmonic energy = " << ratio << " dB vs harmonics");
        CHECK (ratio < -30.0f);
    }
}
