#include <catch2/catch_test_macros.hpp>

#include "SpectralUtils.h"
#include "DSP/WaveguideResonator.h"

using namespace morphium;
using namespace morphium::test;

namespace
{
    // Excites the string with a short seeded noise burst and lets it ring.
    std::vector<float> pluck (WaveguideResonator& string, int burstSamples,
                              int totalSamples, juce::int64 seed = 99)
    {
        juce::Random random (seed);
        std::vector<float> out ((size_t) totalSamples);

        for (int i = 0; i < totalSamples; ++i)
        {
            const float in = (i < burstSamples)
                             ? (random.nextFloat() * 2.0f - 1.0f) * 0.5f
                             : 0.0f;
            out[(size_t) i] = string.processSample (in);
        }
        return out;
    }
}

TEST_CASE ("Waveguide: plucked string rings at the note", "[waveguide][tonal]")
{
    for (double sampleRate : { 44100.0, 96000.0 })
    {
        for (float note : { 110.0f, 440.0f, 1760.0f })
        {
            WaveguideResonator string;
            string.prepare (sampleRate);
            string.setDamping (0.2f, 4.0f);
            string.setFriction (0.2f);
            string.setFrequency (note);

            const auto samples = pluck (string, 256, 32768);
            const auto spectrum = analyse (samples.data(), (int) samples.size(), sampleRate);

            const float f0 = detectF0 (spectrum, note * 0.7f, note * 1.4f);
            const float cents = 1200.0f * std::log2 (f0 / note);

            INFO (note << " Hz @ " << sampleRate << " Hz -> f0 = " << f0
                  << " Hz (" << cents << " cents)");
            CHECK (std::abs (cents) < 15.0f);
        }
    }
}

TEST_CASE ("Waveguide: ring time follows the requested T60", "[waveguide][decay]")
{
    auto ringSeconds = [] (float t60)
    {
        WaveguideResonator string;
        string.prepare (44100.0);
        string.setDamping (0.1f, t60);
        string.setFriction (0.0f);
        string.setFrequency (440.0f);

        const auto samples = pluck (string, 256, 8 * 44100);
        return decayTimeSeconds (samples.data(), (int) samples.size(), 44100.0);
    };

    const float longRing  = ringSeconds (4.0f);
    const float shortRing = ringSeconds (0.5f);

    INFO ("t60 4.0 -> " << longRing << " s, t60 0.5 -> " << shortRing << " s");
    CHECK (longRing > shortRing * 2.0f);
    CHECK (shortRing > 0.1f);
    CHECK (longRing < 8.0f); // it must actually stop
}

TEST_CASE ("Waveguide: continuous drive at resonance stays bounded", "[waveguide][safety]")
{
    // A linear loop driven exactly at resonance grows ~1/(1-g); the in-loop
    // tanh must keep it bounded even in the worst case.
    for (float friction : { 0.0f, 1.0f })
    {
        WaveguideResonator string;
        string.prepare (44100.0);
        string.setDamping (0.0f, 6.0f);   // bright, long sustain: worst case
        string.setFriction (friction);
        string.setFrequency (440.0f);

        float peak = 0.0f;
        bool finite = true;
        double phase = 0.0;
        for (int i = 0; i < 4 * 44100; ++i)
        {
            const float drive = 0.8f * (float) std::sin (phase);
            phase += juce::MathConstants<double>::twoPi * 440.0 / 44100.0;

            const float out = string.processSample (drive);
            finite = finite && std::isfinite (out);
            peak = juce::jmax (peak, std::abs (out));
        }

        INFO ("friction " << friction << " -> peak " << peak);
        CHECK (finite);
        CHECK (peak < 3.0f);
    }
}
