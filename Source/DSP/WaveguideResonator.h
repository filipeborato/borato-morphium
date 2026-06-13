#pragma once

#include <juce_dsp/juce_dsp.h>

#include <vector>

namespace morphium
{
    /**
        The primary resonator: a Karplus-Strong single-delay-loop waveguide.

        This is the "vibrating object" of the instrument — excitation energy is
        injected into the delay line and circulates through a one-pole damping
        filter (brightness/decay) and a soft saturator (friction). Pitch comes
        from the loop length, sustain from the loop gain, brightness from the
        loop filter: timbre follows physics instead of oscillator shapes.

        The in-loop tanh is always active (normalised, so it is transparent at
        small amplitudes). Besides modelling friction it is what keeps
        continuously driven strings (Bow, Breath, Tape) bounded: a linear loop
        driven at resonance would otherwise grow by ~1/(1-g).

        Real-time contract: prepare() allocates the delay line for the lowest
        supported pitch (20 Hz); everything else is allocation-free.
    */
    class WaveguideResonator
    {
    public:
        void prepare (double newSampleRate);
        void reset();

        void setFrequency (float hz) noexcept;

        /** darkening in [0, 1]: 0 = bright/metallic, 1 = heavily damped highs.
            t60 is the ring time in seconds after the excitation stops. */
        void setDamping (float darkening, float t60Seconds) noexcept;

        void setFriction (float friction01) noexcept;

        /** Small multiplier on the loop gain (LFO "bow pressure" tremolo).
            Clamped to [0.95, 1.02]; the in-loop tanh keeps >1 trims bounded. */
        void setGainTrim (float trim) noexcept
        {
            gainTrim = juce::jlimit (0.95f, 1.02f, trim);
        }

        /** Injects one excitation sample and returns the resonator output. */
        float processSample (float excitationIn) noexcept;

    private:
        void updateLoop() noexcept;

        std::vector<float> delayLine;
        int bufferLength = 0;
        int writePos = 0;

        // Fractional-delay read: integer tap + first-order allpass.
        int   tapOffset = 100;
        float apfC = 0.0f, apfX1 = 0.0f, apfY1 = 0.0f;

        // Loop filter + gain.
        float damping = 0.2f;       // one-pole coefficient (darkening)
        float loopGain = 0.999f;
        float gainTrim = 1.0f;
        float loopZ1 = 0.0f;
        float friction = 0.0f;

        float frequency = 440.0f;
        float t60 = 4.0f;
        double sampleRate = 44100.0;
    };
}
