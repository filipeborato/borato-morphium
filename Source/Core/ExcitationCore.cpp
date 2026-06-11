#include "ExcitationCore.h"

namespace morphium
{
namespace
{
    constexpr float twoPi = juce::MathConstants<float>::twoPi;

    inline float fastSine (double normalisedPhase) noexcept
    {
        return std::sin (static_cast<float> (normalisedPhase) * twoPi);
    }

    // Keeps a filter cutoff inside the stable range for the current sample rate.
    inline float clampCutoff (float hz, double sampleRate) noexcept
    {
        const auto nyquist = static_cast<float> (sampleRate * 0.49);
        return juce::jlimit (20.0f, nyquist, hz);
    }
}

juce::StringArray getExcitationTypeNames()
{
    return {
        "STRIKE", "IMPULSE", "PIZZICATO",
        "BOW", "SCRAPE",
        "VOICE", "BREATH",
        "SPARK", "SINE", "WAVETABLE",
        "NOISE",
        "TAPE", "TAPE LOOP"
    };
}

ExcitationCore::ExcitationCore() = default;

void ExcitationCore::prepare (double newSampleRate)
{
    sampleRate = newSampleRate;

    juce::dsp::ProcessSpec spec;
    spec.sampleRate       = sampleRate;
    spec.maximumBlockSize = 512;       // per-sample processing; value is a hint
    spec.numChannels      = 1;

    for (auto* filter : { &noiseBand, &sparkBand, &formantLow, &formantHigh })
    {
        filter->prepare (spec);
        filter->setType (juce::dsp::StateVariableTPTFilterType::bandpass);
    }

    formantLow.setResonance  (3.0f);
    formantHigh.setResonance (4.0f);
    noiseBand.setResonance   (1.5f);
    sparkBand.setResonance   (6.0f);

    setFrequency (frequency);
    reset();
}

void ExcitationCore::reset()
{
    mainPhase = wowPhase = flutterPhase = 0.0;
    driftState = driftTarget = 0.0f;
    driftCounter = 0;
    transientEnv = 0.0f;

    for (auto* filter : { &noiseBand, &sparkBand, &formantLow, &formantHigh })
        filter->reset();
}

void ExcitationCore::setFrequency (float frequencyHz) noexcept
{
    frequency = frequencyHz;
    phaseInc  = frequency / sampleRate;

    // Note-dependent filter tunings.
    noiseBand.setCutoffFrequency  (clampCutoff (frequency * 2.0f, sampleRate));
    sparkBand.setCutoffFrequency  (clampCutoff (frequency * 6.0f, sampleRate));

    // Vowel-like static formants ("ah"), but tracked up high so notes stay lively.
    formantLow.setCutoffFrequency  (clampCutoff (juce::jmax (700.0f, frequency * 1.0f), sampleRate));
    formantHigh.setCutoffFrequency (clampCutoff (juce::jmax (1100.0f, frequency * 2.0f), sampleRate));
}

void ExcitationCore::trigger() noexcept
{
    // Re-arm the transient envelope. Decay length depends on the source.
    const float decaySeconds = (type == ExcitationType::Spark) ? 0.08f : 0.6f;
    transientEnv   = 1.0f;
    transientCoeff = std::exp (-1.0f / static_cast<float> (decaySeconds * sampleRate));
    mainPhase      = 0.0;
}

double ExcitationCore::advancePhase (double& phase, double increment) noexcept
{
    phase += increment;
    while (phase >= 1.0) phase -= 1.0;
    return phase;
}

float ExcitationCore::nextDrift() noexcept
{
    // New random target a few times per second; one-pole glide toward it.
    if (--driftCounter <= 0)
    {
        driftTarget  = random.nextFloat() * 2.0f - 1.0f;
        driftCounter = static_cast<int> (sampleRate * 0.10);
    }

    driftState += (driftTarget - driftState) * 0.0008f;
    return driftState;
}

float ExcitationCore::processSample() noexcept
{
    switch (type)
    {
        // IMPACT
        case ExcitationType::Strike:
        case ExcitationType::Impulse:
        case ExcitationType::Pizzicato: return renderStrike();
        // FRICTION
        case ExcitationType::Bow:
        case ExcitationType::Scrape:    return renderBow();
        // AIR
        case ExcitationType::Voice:
        case ExcitationType::Breath:    return renderVoice();
        // SYNTH
        case ExcitationType::Spark:
        case ExcitationType::Sine:
        case ExcitationType::Wavetable: return renderSpark();
        // NOISE
        case ExcitationType::Noise:     return renderNoise();
        // TAPE
        case ExcitationType::Tape:
        case ExcitationType::TapeLoop:  return renderTape();
        case ExcitationType::NumTypes:
        default:                     return 0.0f;
    }
}

float ExcitationCore::renderBow() noexcept
{
    // Continuously driven tone: small, slow pitch instability + a touch of
    // odd harmonic to suggest the friction of a bow.
    const float drift = nextDrift() * 0.004f;
    advancePhase (mainPhase, phaseInc * (1.0 + drift));

    const float fundamental = fastSine (mainPhase);
    const float third       = fastSine (std::fmod (mainPhase * 3.0, 1.0)) * 0.18f;
    return (fundamental + third) * 0.7f;
}

float ExcitationCore::renderStrike() noexcept
{
    // Impulse exciting a tonal decay: a decaying sine, plus a brief noise
    // click on the very front of the transient.
    transientEnv *= transientCoeff;
    advancePhase (mainPhase, phaseInc);

    const float tone  = fastSine (mainPhase) * transientEnv;
    const float click = (random.nextFloat() * 2.0f - 1.0f)
                        * transientEnv * transientEnv * transientEnv * 0.4f;
    return tone + click;
}

float ExcitationCore::renderTape() noexcept
{
    // Sawtooth with wow (slow) + flutter (fast) pitch modulation, then light
    // tanh saturation for the worn-tape character.
    const double wow     = fastSine (advancePhase (wowPhase,     0.6 / sampleRate)) * 0.010;
    const double flutter = fastSine (advancePhase (flutterPhase, 7.0 / sampleRate)) * 0.003;

    advancePhase (mainPhase, phaseInc * (1.0 + wow + flutter));

    const float saw = static_cast<float> (mainPhase) * 2.0f - 1.0f;
    return std::tanh (saw * 1.8f) * 0.6f;
}

float ExcitationCore::renderVoice() noexcept
{
    // A buzzy pulse fed through two band-passes acting as formants.
    advancePhase (mainPhase, phaseInc);
    const float source = (mainPhase < 0.45) ? 1.0f : -1.0f;   // asymmetric pulse

    const float f1 = formantLow.processSample  (0, source);
    const float f2 = formantHigh.processSample (0, source);
    return (f1 * 0.8f + f2 * 0.5f);
}

float ExcitationCore::renderNoise() noexcept
{
    const float white = random.nextFloat() * 2.0f - 1.0f;
    return noiseBand.processSample (0, white) * 0.9f;
}

float ExcitationCore::renderSpark() noexcept
{
    // Short noisy burst with a bright resonant ping on top.
    transientEnv *= transientCoeff;

    const float white = (random.nextFloat() * 2.0f - 1.0f) * transientEnv;
    const float ping  = sparkBand.processSample (0, white);

    advancePhase (mainPhase, phaseInc * 4.0);
    const float tone = fastSine (mainPhase) * transientEnv * transientEnv * 0.3f;
    return ping + tone;
}
}
