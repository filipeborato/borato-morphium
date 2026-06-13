#include "WaveguideResonator.h"

namespace morphium
{
void WaveguideResonator::prepare (double newSampleRate)
{
    sampleRate = newSampleRate;

    // Long enough for the lowest supported pitch (20 Hz) plus guard samples.
    bufferLength = (int) std::ceil (sampleRate / 20.0) + 8;
    delayLine.assign ((size_t) bufferLength, 0.0f);

    reset();
    updateLoop();
}

void WaveguideResonator::reset()
{
    std::fill (delayLine.begin(), delayLine.end(), 0.0f);
    writePos = 0;
    apfX1 = apfY1 = 0.0f;
    loopZ1 = 0.0f;
}

void WaveguideResonator::setFrequency (float hz) noexcept
{
    const float newFrequency = juce::jlimit (20.0f, (float) (sampleRate * 0.4), hz);
    if (std::abs (newFrequency - frequency) > 0.01f)
    {
        frequency = newFrequency;
        updateLoop();
    }
}

void WaveguideResonator::setDamping (float darkening, float t60Seconds) noexcept
{
    const float newDamping = juce::jlimit (0.0f, 0.75f, darkening);
    const float newT60 = juce::jmax (0.05f, t60Seconds);

    if (std::abs (newDamping - damping) > 1.0e-4f || std::abs (newT60 - t60) > 1.0e-3f)
    {
        damping = newDamping;
        t60 = newT60;
        updateLoop();
    }
}

void WaveguideResonator::setFriction (float friction01) noexcept
{
    friction = juce::jlimit (0.0f, 1.0f, friction01);
}

void WaveguideResonator::updateLoop() noexcept
{
    // Loop gain per circulation for a -60 dB decay in t60 seconds:
    // g^(t60 * f) = 1e-3  =>  g = exp (-3 ln10 / (t60 * f)).
    loopGain = std::exp (-6.9078f / (t60 * frequency));

    // Total loop delay must equal one period. Compensate the one-pole's DC
    // group delay (d / (1 - d) samples), then split into an integer tap plus
    // an allpass fraction kept in [0.1, 1.1) so the allpass stays well
    // conditioned.
    const float total = (float) sampleRate / frequency - damping / (1.0f - damping);

    int   m    = (int) std::floor (total);
    float frac = total - (float) m;
    if (frac < 0.1f)
    {
        m -= 1;
        frac += 1.0f;
    }

    tapOffset = juce::jlimit (1, bufferLength - 2, m);
    apfC = (1.0f - frac) / (1.0f + frac);
}

float WaveguideResonator::processSample (float excitationIn) noexcept
{
    int readPos = writePos - tapOffset;
    if (readPos < 0)
        readPos += bufferLength;

    // Fractional delay: first-order allpass interpolator.
    const float tap = delayLine[(size_t) readPos];
    const float delayed = apfC * tap + apfX1 - apfC * apfY1;
    apfX1 = tap;
    apfY1 = delayed;

    // Loop filter: one-pole lowpass — the per-period high-frequency loss of
    // the vibrating body.
    loopZ1 = delayed + damping * (loopZ1 - delayed);

    float feedback = loopZ1 * loopGain * gainTrim;

    // In-loop saturation: transparent at small amplitude (normalised tanh),
    // it limits loud circulation — this is both the friction character and
    // the energy bound for continuously driven strings.
    const float bodyDrive = 1.0f + friction * 3.0f;
    feedback = std::tanh (feedback * bodyDrive) / bodyDrive;

    delayLine[(size_t) writePos] = excitationIn + feedback;
    if (++writePos >= bufferLength)
        writePos = 0;

    return loopZ1;
}
}
