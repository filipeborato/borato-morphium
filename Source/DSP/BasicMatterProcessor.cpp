#include "BasicMatterProcessor.h"

namespace morphium
{
namespace
{
    inline float clampCutoff (float hz, double sampleRate) noexcept
    {
        return juce::jlimit (20.0f, static_cast<float> (sampleRate * 0.49), hz);
    }
}

BasicMatterProcessor::BasicMatterProcessor() = default;

void BasicMatterProcessor::prepare (double newSampleRate)
{
    sampleRate = newSampleRate;

    juce::dsp::ProcessSpec spec;
    spec.sampleRate       = sampleRate;
    spec.maximumBlockSize = 512;
    spec.numChannels      = 1;

    lowpass.prepare (spec);
    lowpass.setType (juce::dsp::StateVariableTPTFilterType::lowpass);

    highpass.prepare (spec);
    highpass.setType (juce::dsp::StateVariableTPTFilterType::highpass);

    const double rampSeconds = 0.02;
    for (auto* s : { &densitySm, &massSm, &frictionSm, &wearSm })
        s->reset (sampleRate, rampSeconds);

    reset();
}

void BasicMatterProcessor::reset()
{
    lowpass.reset();
    highpass.reset();
    for (auto& r : resonators)
        r.reset();
    dampState = 0.0f;
    lastLowpassHz = lastHighpassHz = -1.0f;
}

void BasicMatterProcessor::setParameters (float density, float mass, float friction, float wear) noexcept
{
    densitySm.setTargetValue  (juce::jlimit (0.0f, 1.0f, density));
    massSm.setTargetValue     (juce::jlimit (0.0f, 1.0f, mass));
    frictionSm.setTargetValue (juce::jlimit (0.0f, 1.0f, friction));
    wearSm.setTargetValue     (juce::jlimit (0.0f, 1.0f, wear));
}

void BasicMatterProcessor::setResonatorParameters (int mode, float baseFreq) noexcept
{
    resonatorMode = mode;
    baseFreqHz = baseFreq;
}

float BasicMatterProcessor::processSample (float input) noexcept
{
    const float density  = densitySm.getNextValue();
    const float mass     = massSm.getNextValue();
    const float friction = frictionSm.getNextValue();
    const float wear     = wearSm.getNextValue();

    // Mass: heavier matter is darker. Map to a low-pass that closes down.
    const float lpHz = clampCutoff (juce::jmap (mass, 18000.0f, 350.0f), sampleRate);
    if (std::abs (lpHz - lastLowpassHz) > 0.5f)
    {
        lowpass.setCutoffFrequency (lpHz);
        lastLowpassHz = lpHz;
    }

    // Density: more density keeps the low body; less makes it thin.
    const float hpHz = clampCutoff (juce::jmap (density, 600.0f, 20.0f), sampleRate);
    if (std::abs (hpHz - lastHighpassHz) > 0.5f)
    {
        highpass.setCutoffFrequency (hpHz);
        lastHighpassHz = hpHz;
    }

    float x = highpass.processSample (0, input);
    x = lowpass.processSample (0, x);

    // Friction: saturation drive, normalised so loudness stays roughly stable.
    const float drive = 1.0f + friction * 8.0f;
    x = std::tanh (x * drive) / std::tanh (drive);

    // Wear: amplitude instability + a little high-frequency damping.
    const float noise = random.nextFloat() * 2.0f - 1.0f;
    x *= (1.0f - wear * 0.5f * std::abs (noise));

    // Modal Resonator Bank (METAL / GLASS / WOOD)
    if (resonatorMode > 0)
    {
        std::array<float, numModes> ratios { 1.0f, 2.0f, 3.0f, 4.0f };
        float baseQ = 50.0f;

        if (resonatorMode == 1) // METAL
        {
            ratios = { 1.0f, 1.62f, 2.31f, 3.75f };
            baseQ = 220.0f * (1.0f - wear * 0.75f);
        }
        else if (resonatorMode == 2) // GLASS
        {
            ratios = { 1.0f, 2.0f, 3.14f, 4.07f };
            baseQ = 400.0f * (1.0f - wear * 0.85f);
        }
        else if (resonatorMode == 3) // WOOD
        {
            ratios = { 1.0f, 1.46f, 1.98f, 2.65f };
            baseQ = 30.0f * (1.0f - wear * 0.60f);
        }

        float resonatorSum = 0.0f;
        for (int i = 0; i < numModes; ++i)
        {
            const float f = clampCutoff (baseFreqHz * ratios[(size_t) i], sampleRate);
            const float q = juce::jmax (2.0f, baseQ * (1.0f - (float) i * 0.15f));
            resonators[(size_t) i].updateCoefficients (f, q, sampleRate);
            resonatorSum += resonators[(size_t) i].process (x);
        }

        // Blend resonance back into physical body
        x = x * 0.40f + resonatorSum * 0.60f;
    }

    const float dampCoeff = juce::jmap (wear, 1.0f, 0.25f); // worn = more damped
    dampState += (x - dampState) * dampCoeff;

    // Light makeup so friction at zero is not noticeably quieter.
    return dampState * 1.2f;
}
}
