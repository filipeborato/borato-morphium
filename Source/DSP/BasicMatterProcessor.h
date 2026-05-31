#pragma once

#include <juce_dsp/juce_dsp.h>

namespace morphium
{
    /**
        Shapes the raw excitation into "matter".

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

        float processSample (float input) noexcept;

    private:
        double sampleRate = 44100.0;

        juce::dsp::StateVariableTPTFilter<float> lowpass;   // mass
        juce::dsp::StateVariableTPTFilter<float> highpass;  // density

        juce::SmoothedValue<float> densitySm, massSm, frictionSm, wearSm;

        // Avoids recomputing filter coefficients when the cutoff is unchanged.
        float lastLowpassHz  = -1.0f;
        float lastHighpassHz = -1.0f;

        float dampState = 0.0f;
        juce::Random random;
    };
}
