#include "BasicMatterProcessor.h"

namespace morphium
{
namespace
{
    inline float clampCutoff (float hz, double sampleRate) noexcept
    {
        return juce::jlimit (20.0f, static_cast<float> (sampleRate * 0.49), hz);
    }

    struct Mode
    {
        float ratio;     // frequency as a multiple of the fundamental
        float gainDb;    // relative level
        float t60;       // ring time in seconds (at wear = 0)
    };

    struct MaterialTable
    {
        std::array<Mode, BasicMatterProcessor::numModes> modes;
        float wearT60Loss;     // fraction of the ring time wear can eat away
        float inharmonicity;   // density-driven mode-spread factor
    };

    // Index 0..5 = METAL / GLASS / WOOD / TAPE / CIRCUIT / CONCRETE.
    // Ratios and ring times are stylised from measured modal data: METAL is
    // inharmonic and long, GLASS quasi-harmonic and very long, WOOD warm and
    // short, TAPE flutter-flat and tight, CIRCUIT stretched-harmonic,
    // CONCRETE clustered and dense.
    constexpr MaterialTable materialTables[BasicMatterProcessor::numMaterials] = {
        // METAL
        { { { { 1.000f,   0.0f, 4.00f }, { 1.620f,  -3.0f, 3.20f },
              { 2.310f,  -5.0f, 2.50f }, { 3.750f,  -7.0f, 2.00f },
              { 5.120f,  -9.0f, 1.50f }, { 6.810f, -11.0f, 1.20f },
              { 9.070f, -14.0f, 0.90f }, { 11.40f, -18.0f, 0.60f } } },
          0.75f, 0.40f },
        // GLASS
        { { { { 1.000f,   0.0f, 8.00f }, { 2.000f,  -2.0f, 7.00f },
              { 3.140f,  -4.0f, 5.50f }, { 4.070f,  -6.0f, 4.00f },
              { 5.920f,  -8.0f, 3.00f }, { 7.810f, -11.0f, 2.00f },
              { 9.430f, -14.0f, 1.20f }, { 11.05f, -18.0f, 0.70f } } },
          0.85f, 0.10f },
        // WOOD
        { { { { 1.000f,   0.0f, 0.80f }, { 1.460f,  -3.0f, 0.60f },
              { 1.980f,  -5.0f, 0.40f }, { 2.650f,  -7.0f, 0.30f },
              { 3.500f,  -9.0f, 0.20f }, { 4.200f, -12.0f, 0.15f },
              { 5.500f, -15.0f, 0.10f }, { 6.800f, -19.0f, 0.07f } } },
          0.60f, 0.30f },
        // TAPE — ratios just under the harmonics: flutter detune
        { { { { 1.000f,   0.0f, 0.15f }, { 1.990f,  -2.0f, 0.12f },
              { 2.985f,  -4.0f, 0.10f }, { 3.970f,  -6.0f, 0.08f },
              { 4.950f,  -8.0f, 0.06f }, { 5.920f, -11.0f, 0.04f },
              { 6.890f, -14.0f, 0.03f }, { 7.850f, -18.0f, 0.02f } } },
          0.50f, 0.05f },
        // CIRCUIT — slightly stretched harmonics: parasitic resonance
        { { { { 1.000f,   0.0f, 0.50f }, { 2.001f,  -1.0f, 0.40f },
              { 3.003f,  -3.0f, 0.30f }, { 4.007f,  -5.0f, 0.25f },
              { 5.012f,  -7.0f, 0.20f }, { 6.019f, -10.0f, 0.15f },
              { 7.028f, -13.0f, 0.10f }, { 8.040f, -17.0f, 0.07f } } },
          0.50f, 0.10f },
        // CONCRETE — modes cluster instead of spreading
        { { { { 1.000f,   0.0f, 1.50f }, { 1.850f,  -2.0f, 1.00f },
              { 2.600f,  -5.0f, 0.70f }, { 3.200f,  -8.0f, 0.50f },
              { 3.750f, -10.0f, 0.35f }, { 4.100f, -13.0f, 0.25f },
              { 4.400f, -16.0f, 0.15f }, { 4.650f, -20.0f, 0.10f } } },
          0.50f, 0.20f },
    };

    // Q for a 2-pole resonator whose impulse response decays 60 dB in t60
    // seconds: envelope = exp(-pi*f*t/Q)  =>  Q = pi*f*t60 / (3*ln 10).
    inline float qFromT60 (float freqHz, float t60) noexcept
    {
        return juce::MathConstants<float>::pi * freqHz * t60 / 6.9078f;
    }
}

BasicMatterProcessor::BasicMatterProcessor()
{
    gateState.fill (1.0f);
    gateTarget.fill (1.0f);
    modeGain.fill (1.0f);
}

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

    // One-pole DC blocker tuned to ~8 Hz.
    dcR = 1.0f - juce::MathConstants<float>::twoPi * 8.0f / static_cast<float> (sampleRate);

    reset();
}

void BasicMatterProcessor::reset()
{
    lowpass.reset();
    highpass.reset();
    for (auto& r : resonators)
        r.reset();

    dampState = 0.0f;
    dcX1 = dcY1 = 0.0f;
    lastLowpassHz = lastHighpassHz = -1.0f;

    detuneState.fill (0.0f);
    detuneTarget.fill (0.0f);
    gateState.fill (1.0f);
    gateTarget.fill (1.0f);
    controlCounter = 0;

    // Force a coefficient refresh on the next setResonatorParameters call.
    lastMode = -1;
    lastBaseFreqHz = -1.0f;
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
    resonatorMode = juce::jlimit (0, numMaterials, mode);
    baseFreqHz = baseFreq;

    if (resonatorMode != lastMode || std::abs (baseFreq - lastBaseFreqHz) > 0.5f)
    {
        lastMode = resonatorMode;
        lastBaseFreqHz = baseFreq;
        updateModalCoefficients();
    }
}

void BasicMatterProcessor::updateModalCoefficients() noexcept
{
    if (resonatorMode <= 0)
        return;

    const auto& table = materialTables[(size_t) resonatorMode - 1];
    const float density = densitySm.getTargetValue();
    const float mass    = massSm.getTargetValue();
    const float wear    = wearSm.getTargetValue();

    // Density: less dense matter lets the modes drift apart (inharmonicity).
    const float spread = 1.0f + (1.0f - density) * table.inharmonicity;

    // Wear: aged matter loses ring time.
    const float t60Scale = 1.0f - wear * table.wearT60Loss;

    for (int i = 0; i < numModes; ++i)
    {
        const auto& mode = table.modes[(size_t) i];

        const float ratio  = 1.0f + (mode.ratio - 1.0f) * spread;
        const float detune = std::exp2 (detuneState[(size_t) i] / 12.0f);
        const float f = clampCutoff (baseFreqHz * ratio * detune, sampleRate);
        const float q = juce::jmax (2.0f, qFromT60 (f, juce::jmax (0.01f, mode.t60 * t60Scale)));
        resonators[(size_t) i].updateCoefficients (f, q, sampleRate);

        // Mass: spectral tilt — massive matter swallows its upper modes.
        modeGain[(size_t) i] = juce::Decibels::decibelsToGain (
            mode.gainDb - mass * 5.0f * (float) i);
    }
}

void BasicMatterProcessor::updateControlRateModulation (float wear) noexcept
{
    // Slow random walk of each mode's tuning: aged matter wanders, it does
    // not hiss. Depth reaches ~±35 cents at full wear.
    for (int i = 0; i < numModes; ++i)
    {
        if (random.nextFloat() < 0.10f)
            detuneTarget[(size_t) i] = (random.nextFloat() * 2.0f - 1.0f) * wear * 0.35f;

        detuneState[(size_t) i] += (detuneTarget[(size_t) i] - detuneState[(size_t) i]) * 0.05f;
    }

    // Occasional soft mode dropouts once the material is badly worn.
    const float dropoutChance = juce::jmax (0.0f, wear - 0.7f) / 0.3f * 0.05f;
    for (int i = 1; i < numModes; ++i) // never silence the fundamental mode
    {
        if (gateTarget[(size_t) i] < 1.0f && random.nextFloat() < 0.15f)
            gateTarget[(size_t) i] = 1.0f;            // recover
        else if (random.nextFloat() < dropoutChance)
            gateTarget[(size_t) i] = 0.15f;           // soft dropout, not silence
    }

    // Density/mass may be moving (LFO drift), and the detune walk above needs
    // applying — refresh the bank. 8 modes' worth of transcendentals every 256
    // samples is negligible.
    updateModalCoefficients();
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

    // The saturation of asymmetric waveforms rectifies them slightly; block
    // the resulting DC before it reaches the resonators and the output.
    const float dcOut = x - dcX1 + dcR * dcY1;
    dcX1 = x;
    dcY1 = dcOut;
    x = dcOut;

    // Modal resonator bank.
    if (resonatorMode > 0)
    {
        if (--controlCounter <= 0)
        {
            controlCounter = controlPeriod;
            updateControlRateModulation (wear);
        }

        float resonatorSum = 0.0f;
        for (int i = 0; i < numModes; ++i)
        {
            auto& gate = gateState[(size_t) i];
            gate += (gateTarget[(size_t) i] - gate) * 0.001f;   // click-free dropouts
            resonatorSum += resonators[(size_t) i].process (x) * gate * modeGain[(size_t) i];
        }

        // Friction also makes the *body* nonlinear: loud rings saturate.
        // tanh(x*d)/d has unity small-signal gain, so quiet passages stay
        // untouched; the blend keeps friction = 0 perfectly linear.
        const float bodyDrive = 1.0f + friction * 4.0f;
        const float saturated = std::tanh (resonatorSum * bodyDrive) / bodyDrive;
        resonatorSum += friction * (saturated - resonatorSum);

        // Blend resonance back into the physical body.
        x = x * 0.40f + resonatorSum * 0.60f;
    }

    const float dampCoeff = juce::jmap (wear, 1.0f, 0.25f); // worn = more damped
    dampState += (x - dampState) * dampCoeff;

    // Light makeup so friction at zero is not noticeably quieter.
    return dampState * 1.2f;
}
}
