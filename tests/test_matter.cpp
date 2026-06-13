#include <catch2/catch_test_macros.hpp>

#include "SpectralUtils.h"
#include "DSP/BasicMatterProcessor.h"

using namespace morphium;
using namespace morphium::test;

namespace
{
    struct MatterParams
    {
        float density = 1.0f;    // HP fully open (20 Hz)
        float mass = 0.0f;       // LP fully open (18 kHz)
        float friction = 0.0f;   // tanh ~ linear
        float wear = 0.0f;       // no instability, no damping
        int resonatorMode = 0;
        float baseFreq = 440.0f;
    };

    // Runs the matter stage over an input signal. A silent warm-up lets the
    // 20 ms parameter smoothers settle on their targets first.
    std::vector<float> renderMatter (const std::vector<float>& input,
                                     const MatterParams& params,
                                     double sampleRate = 44100.0,
                                     juce::int64 seed = 777)
    {
        BasicMatterProcessor matter;
        matter.prepare (sampleRate);
        matter.setSeed (seed);
        matter.setParameters (params.density, params.mass, params.friction, params.wear);
        matter.setResonatorParameters (params.resonatorMode, params.baseFreq);

        for (int i = 0; i < 4096; ++i)
            matter.processSample (0.0f);

        std::vector<float> out (input.size());
        for (size_t i = 0; i < input.size(); ++i)
            out[i] = matter.processSample (input[i]);
        return out;
    }

    std::vector<float> impulse (int numSamples)
    {
        std::vector<float> signal ((size_t) numSamples, 0.0f);
        signal[0] = 1.0f;
        return signal;
    }

    std::vector<float> seededNoise (int numSamples, juce::int64 seed = 42)
    {
        juce::Random random (seed);
        std::vector<float> signal ((size_t) numSamples);
        for (auto& s : signal)
            s = random.nextFloat() * 2.0f - 1.0f;
        return signal;
    }
}

TEST_CASE ("Matter: modal peaks land on the material's frequency ratios", "[matter][modal]")
{
    // Ratio tables mirror materialTables in BasicMatterProcessor.cpp; the test
    // pins the audible contract (a mode rings at f0 * ratio), not the code.
    struct Material { int mode; const char* name; std::vector<float> ratios; float tolHz; };
    const Material materials[] = {
        { 1, "METAL",    { 1.0f, 1.62f, 2.31f, 3.75f, 5.12f, 6.81f, 9.07f, 11.4f }, 5.0f },
        { 2, "GLASS",    { 1.0f, 2.0f, 3.14f, 4.07f, 5.92f, 7.81f, 9.43f, 11.05f }, 5.0f },
        { 3, "WOOD",     { 1.0f, 1.46f, 1.98f, 2.65f, 3.5f, 4.2f }, 8.0f },   // low-Q = broad
        { 4, "TAPE",     { 1.0f, 1.99f, 2.985f, 3.97f }, 10.0f },             // very low Q
        { 5, "CIRCUIT",  { 1.0f, 2.001f, 3.003f, 4.007f, 5.012f }, 6.0f },
        { 6, "CONCRETE", { 1.0f, 1.85f, 2.6f, 3.2f }, 6.0f },
    };

    for (const auto& material : materials)
    {
        MatterParams params;
        params.resonatorMode = material.mode;

        const auto out = renderMatter (impulse (32768), params);
        const auto spectrum = analyse (out.data(), (int) out.size(), 44100.0);
        const auto peaks = findPeaks (spectrum, -50.0f, 100.0f, 8000.0f);

        for (float ratio : material.ratios)
        {
            const float expected = 440.0f * ratio;
            bool found = false;
            for (const auto& peak : peaks)
                if (std::abs (peak.freqHz - expected) <= material.tolHz)
                    found = true;

            INFO (material.name << ": expected mode at " << expected << " Hz");
            CHECK (found);
        }
    }
}

TEST_CASE ("Matter: density spreads the METAL modes apart", "[matter][modal]")
{
    // Inharmonicity contract: at density 0 the METAL mode 2 (ratio 1.62,
    // spread factor 0.4) must sit ~25% higher than at density 1.
    auto mode2Freq = [] (float density)
    {
        MatterParams params;
        params.resonatorMode = 1; // METAL
        params.density = density;

        const auto out = renderMatter (impulse (32768), params);
        const auto spectrum = analyse (out.data(), (int) out.size(), 44100.0);
        return detectF0 (spectrum, 600.0f, 1100.0f); // search around mode 2
    };

    const float dense = mode2Freq (1.0f);   // expected ~713 Hz (440 * 1.62)
    const float loose = mode2Freq (0.0f);   // expected ~787 Hz (spread 1.4)

    INFO ("METAL mode 2: density 1 -> " << dense << " Hz, density 0 -> " << loose << " Hz");
    CHECK (std::abs (dense - 440.0f * 1.62f) < 8.0f);
    CHECK (loose > dense * 1.05f);
}

TEST_CASE ("Matter: spectral centroid falls monotonically with mass", "[matter][tonal]")
{
    const auto noise = seededNoise (32768);

    std::vector<float> centroids;
    for (float mass : { 0.0f, 0.25f, 0.5f, 0.75f, 1.0f })
    {
        MatterParams params;
        params.mass = mass;

        const auto out = renderMatter (noise, params);
        centroids.push_back (spectralCentroidHz (
            analyse (out.data(), (int) out.size(), 44100.0)));
    }

    for (size_t i = 1; i < centroids.size(); ++i)
    {
        INFO ("centroid[" << i - 1 << "] = " << centroids[i - 1]
              << " Hz, centroid[" << i << "] = " << centroids[i] << " Hz");
        CHECK (centroids[i] <= centroids[i - 1] * 1.02f);
    }

    CHECK (centroids.front() - centroids.back() >= 500.0f);
}

TEST_CASE ("Matter: low-frequency body grows with density", "[matter][tonal]")
{
    const auto noise = seededNoise (32768);

    std::vector<float> lowBandDb;
    for (float density : { 0.0f, 0.5f, 1.0f })
    {
        MatterParams params;
        params.density = density;

        const auto out = renderMatter (noise, params);
        lowBandDb.push_back (bandEnergyDb (
            analyse (out.data(), (int) out.size(), 44100.0), 30.0f, 300.0f));
    }

    INFO ("low band: density 0 = " << lowBandDb[0] << " dB, 0.5 = "
          << lowBandDb[1] << " dB, 1 = " << lowBandDb[2] << " dB");
    CHECK (lowBandDb[1] > lowBandDb[0]);
    CHECK (lowBandDb[2] > lowBandDb[1]);
    CHECK (lowBandDb[2] - lowBandDb[0] >= 6.0f);
}

TEST_CASE ("Matter: wear shortens the GLASS resonator's ring", "[matter][wear]")
{
    std::vector<float> decays;
    for (float wear : { 0.0f, 0.5f, 1.0f })
    {
        MatterParams params;
        params.resonatorMode = 2; // GLASS
        params.wear = wear;

        // GLASS rings for ~8 s at wear 0; the window must outlast the ring.
        const auto out = renderMatter (impulse (10 * 44100), params);
        decays.push_back (decayTimeSeconds (out.data(), (int) out.size(), 44100.0));
    }

    INFO ("decay to -60 dB: wear 0 = " << decays[0] << " s, 0.5 = "
          << decays[1] << " s, 1 = " << decays[2] << " s");
    CHECK (decays[0] > decays[1]);
    CHECK (decays[1] > decays[2]);
    // The -60 dB point is referenced to the envelope peak (dominated by the
    // dry transient), so the measured ratio understates the modal T60 change;
    // 2.5x is the calibrated bound (measured ~2.8x).
    CHECK (decays[0] >= decays[2] * 2.5f);
}
