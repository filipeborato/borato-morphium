#pragma once

#include <juce_dsp/juce_dsp.h>

#include <array>

namespace morphium
{
    /**
        A single 2-pole resonator (one vibrational mode of the material).
        Coefficients are computed outside the audio loop — see
        BasicMatterProcessor::updateModalCoefficients().
    */
    struct ModalResonator
    {
        float y1 = 0.0f, y2 = 0.0f;
        float b1 = 0.0f, b2 = 0.0f, a = 0.0f;

        void updateCoefficients (float f, float Q, double sampleRate) noexcept
        {
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
        Shapes the raw excitation into "matter": an 8-mode modal resonator bank
        with per-material mode tables (frequency ratio, gain, T60), driven by
        four physical controls:

          - density  : low-end body (high-pass) + modal inharmonicity spread
                       (less dense matter -> modes drift apart, more metallic)
          - mass     : darkness (low-pass) + spectral tilt of the mode gains
                       (massive matter -> low modes dominate)
          - friction : exciter saturation drive + nonlinearity of the
                       resonant body itself (loud rings distort)
          - wear     : material ageing — modal T60 loss, slow random mode
                       detune, occasional soft mode dropouts (never noise)

        Materials: 0=OFF, 1=METAL, 2=GLASS, 3=WOOD, 4=TAPE, 5=CIRCUIT,
        6=CONCRETE.

        Real-time contract: prepare() allocates; everything else is
        allocation-free. Modal coefficients are recomputed at control rate
        (every 256 samples) or when the note/model changes, never per sample.
    */
    class BasicMatterProcessor
    {
    public:
        BasicMatterProcessor();

        void prepare (double newSampleRate);
        void reset();

        /** Sets the smoothed targets. Values are expected in [0, 1]. */
        void setParameters (float density, float mass, float friction, float wear) noexcept;

        /** Sets the resonator material (see class comment) and base
            fundamental frequency. Recomputes the modal coefficients when
            either actually changed — call freely at block rate. */
        void setResonatorParameters (int mode, float baseFreq) noexcept;

        /** Test hook: reseeds the internal RNG so offline renders are deterministic. */
        void setSeed (juce::int64 seed) noexcept { random.setSeed (seed); }

        float processSample (float input) noexcept;

        static constexpr int numModes = 8;
        static constexpr int numMaterials = 6;   // excluding OFF

    private:
        void updateModalCoefficients() noexcept;
        void updateControlRateModulation (float wear) noexcept;

        double sampleRate = 44100.0;

        juce::dsp::StateVariableTPTFilter<float> lowpass;   // mass
        juce::dsp::StateVariableTPTFilter<float> highpass;  // density

        juce::SmoothedValue<float> densitySm, massSm, frictionSm, wearSm;

        // Avoids recomputing filter coefficients when the cutoff is unchanged.
        float lastLowpassHz  = -1.0f;
        float lastHighpassHz = -1.0f;

        // DC blocker after the saturation stage (tanh of asymmetric waveforms
        // such as the Tape saw otherwise leaves an audible DC offset).
        float dcR = 0.999f;
        float dcX1 = 0.0f, dcY1 = 0.0f;

        // Modal resonator bank.
        std::array<ModalResonator, numModes> resonators;
        std::array<float, numModes> modeGain {};      // table gain x mass tilt
        int   resonatorMode = 0;
        float baseFreqHz = 440.0f;
        int   lastMode = -1;
        float lastBaseFreqHz = -1.0f;

        // Control-rate modulation state, updated every controlPeriod samples.
        static constexpr int controlPeriod = 256;
        int controlCounter = 0;
        std::array<float, numModes> detuneState {};   // semitone offsets
        std::array<float, numModes> detuneTarget {};
        std::array<float, numModes> gateState;        // 1 = mode audible
        std::array<float, numModes> gateTarget;

        float dampState = 0.0f;
        juce::Random random;
    };
}
