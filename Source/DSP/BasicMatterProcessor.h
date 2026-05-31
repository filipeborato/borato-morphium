#pragma once

#include <juce_dsp/juce_dsp.h>

namespace morphium
{
    /**
        Shapes the raw excitation into "matter".

        The four controls are deliberately physical rather than electronic, so
        that later we can swap this for real material models (Tape, Metal,
        Glass, Circuit, Concrete) behind the same interface.
    */
    struct ModalResonator
    {
        float y1 = 0.0f, y2 = 0.0f;
        float freq = 1000.0f;
        float q = 100.0f;
        float b1 = 0.0f, b2 = 0.0f, a = 0.0f;

        void updateCoefficients (float f, float Q, double sampleRate) noexcept
        {
            freq = f;
            q = Q;
            const float theta = juce::MathConstants<float>::twoPi * f / static_cast<float> (sampleRate);
            const float r = std::exp (-theta / (2.0f * Q));
            b1 = -2.0f * r * std::cos (theta);
            b2 = r * r;
            a = (1.0f - r * r) * 0.5f; // bandpass scaling for unity gain
        }

        void reset() noexcept
        {
            y1 = y2 = 0.0f;
        }

        float process (float x) noexcept
        {
            const float y = a * x - b1 * y1 - b2 * y2;
            y2 = y1;
            y1 = y;
            return y;
        }
    };

    /**
        Shapes the raw excitation into "matter" and adds Modal Resonance.

        The four controls are deliberately physical rather than electronic, so
        that later we can swap this for real material models (Tape, Metal,
        Glass, Circuit, Concrete) behind the same interface:

          - density  : body / low-end presence (high-pass opening up)
          - mass     : weight / darkness (low-pass closing down)
          - friction : saturation drive
          - wear     : amplitude instability and high-frequency damping

        Real-time contract: prepare() allocates; setParameters() and
        processSample() are allocation-free.
    */
    class BasicMatterProcessor
    {
    public:
        BasicMatterProcessor();

        void prepare (double newSampleRate);
        void reset();

        /** Sets the smoothed targets. Values are expected in [0, 1]. */
        void setParameters (float density, float mass, float friction, float wear) noexcept;

        /** Sets the resonator model (0=OFF, 1=METAL, 2=GLASS, 3=WOOD) and base fundamental frequency. */
        void setResonatorParameters (int mode, float baseFreq) noexcept;

        float processSample (float input) noexcept;

    private:
        double sampleRate = 44100.0;

        juce::dsp::StateVariableTPTFilter<float> lowpass;   // mass
        juce::dsp::StateVariableTPTFilter<float> highpass;  // density

        juce::SmoothedValue<float> densitySm, massSm, frictionSm, wearSm;

        // Avoids recomputing filter coefficients when the cutoff is unchanged.
        float lastLowpassHz  = -1.0f;
        float lastHighpassHz = -1.0f;

        // Modal Resonator Bank
        static constexpr int numModes = 4;
        std::array<ModalResonator, numModes> resonators;
        int resonatorMode = 0;
        float baseFreqHz = 440.0f;

        float dampState = 0.0f;
        juce::Random random;
    };
}
