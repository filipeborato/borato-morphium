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

    const float dampCoeff = juce::jmap (wear, 1.0f, 0.25f); // worn = more damped
    dampState += (x - dampState) * dampCoeff;

    // Light makeup so friction at zero is not noticeably quieter.
    return dampState * 1.2f;
}
}
