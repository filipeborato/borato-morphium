#pragma once

#include <juce_dsp/juce_dsp.h>

namespace morphium
{
    /**
        The six "sound matter" energy sources.

        These are not classic oscillator shapes: each one models a *way of
        injecting energy* into the instrument. The implementations are still
        simple for the MVP, but the type list and dispatch are the stable API
        future excitation models will grow into.
    */
    enum class ExcitationType
    {
        // IMPACT
        Strike, Impulse, Pizzicato,
        // FRICTION
        Bow, Scrape,
        // AIR
        Voice, Breath,
        // SYNTH
        Spark, Sine, Wavetable,
        // NOISE
        Noise,
        // TAPE
        Tape, TapeLoop,
        
        NumTypes
    };

    /** Display names, index-aligned with ExcitationType. */
    juce::StringArray getExcitationTypeNames();

    /**
        Generates one mono excitation signal per voice.

        Real-time contract: prepare() does all allocation; processSample(),
        setType(), setFrequency() and trigger() are allocation-free and safe
        to call from the audio thread.
    */
    class ExcitationCore
    {
    public:
        ExcitationCore();

        void prepare (double newSampleRate);
        void reset();

        void setType (ExcitationType newType) noexcept { type = newType; }
        void setFrequency (float frequencyHz) noexcept;

        /** Re-arms transient-based sources (Strike, Spark) on a new note. */
        void trigger() noexcept;

        float processSample() noexcept;

    private:
        float renderBow() noexcept;
        float renderStrike() noexcept;
        float renderTape() noexcept;
        float renderVoice() noexcept;
        float renderNoise() noexcept;
        float renderSpark() noexcept;

        // Advances and wraps a normalised phase [0, 1), returning the new value.
        static double advancePhase (double& phase, double increment) noexcept;

        // Slow random-walk used for organic instability/drift.
        float nextDrift() noexcept;

        double sampleRate = 44100.0;
        ExcitationType type = ExcitationType::Bow;

        float  frequency = 220.0f;
        double phaseInc  = 0.0;     // main oscillator increment per sample

        double mainPhase    = 0.0;
        double wowPhase     = 0.0;
        double flutterPhase = 0.0;

        // Smoothed random walk state for Bow/Tape drift.
        float driftState   = 0.0f;
        float driftTarget  = 0.0f;
        int   driftCounter = 0;

        // Transient envelope shared by Strike/Spark.
        float transientEnv   = 0.0f;
        float transientCoeff = 0.0f;

        juce::Random random;

        juce::dsp::StateVariableTPTFilter<float> noiseBand;
        juce::dsp::StateVariableTPTFilter<float> sparkBand;
        juce::dsp::StateVariableTPTFilter<float> formantLow;
        juce::dsp::StateVariableTPTFilter<float> formantHigh;
    };
}
