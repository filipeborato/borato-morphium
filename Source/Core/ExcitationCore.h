#pragma once

#include <juce_dsp/juce_dsp.h>

namespace morphium
{
    /**
        The "sound matter" energy sources.

        These are not classic oscillator shapes: each one models a *way of
        injecting energy* into the instrument, and every entry below is now its
        own distinct model (no aliasing between types) so the SOURCE selector
        actually changes the physics of the excitation:

          IMPACT   Strike      mallet — decaying tone + soft noise click
                   Impulse     near-dirac broadband click (ideal resonator pluck)
                   Pizzicato   short plucked body: noisy front + fast tonal decay
          FRICTION Bow         steady tone, gentle pitch drift, odd harmonic
                   Scrape      rough bow: deep drift, rosin grit, stronger odds
          AIR      Voice       buzzy pulse through vowel formants
                   Breath      mostly air noise + faint pitched whistle
          SYNTH    Spark       electric burst + bright resonant ping
                   Sine        pure fundamental (clean sub / test tone)
                   Wavetable   real band-limited wavetable — harmonic morph (sine..square)
          NOISE    Noise       band-passed white around 2*f0
          TAPE     Tape        saw + wow/flutter + record-head rolloff + sat
                   TapeLoop    Tape plus heavier flutter and a worn-splice dropout
          SYNTH    WtPwm       wavetable — pulse-width morph
          (wave-   WtFormant   wavetable — vowel/formant sweep
           table)  WtMetallic  wavetable — clangy bright digital
                   WtEPiano    wavetable — electric-piano tine cluster
                   WtDigital   wavetable — PPG-style digital morph

        The WAVE knob (params::wavePosition) scans the morph frames of whichever
        wavetable shape is selected. The five Wt* entries are appended after the
        original list so existing preset excitation indices stay stable.
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
        // SYNTH (wavetable family) — appended to keep existing preset indices stable
        WtPwm, WtFormant, WtMetallic, WtEPiano, WtDigital,

        NumTypes
    };

    /** Display names, index-aligned with ExcitationType. */
    juce::StringArray getExcitationTypeNames();

    /** True for the wavetable shapes (the only sources the WAVE scan knob
        affects). The editor uses this to show the WAVE knob only when relevant. */
    bool isWavetableType (ExcitationType type) noexcept;
    inline bool isWavetableType (int typeIndex) noexcept
    {
        return isWavetableType (static_cast<ExcitationType> (typeIndex));
    }

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

        void setType (ExcitationType newType) noexcept
        {
            type    = newType;
            wtShape = wavetableShapeFor (newType);
        }

        void setFrequency (float frequencyHz) noexcept;

        /** Wavetable scan position (0..1), driven by the WAVE knob + Motion LFO.
            Ignored by non-wavetable sources. */
        void setMorphPosition (float pos01) noexcept
        {
            wtMorphBias = juce::jlimit (0.0f, 1.0f, pos01);
        }

        /** Re-arms transient-based sources (Strike, Spark) on a new note. */
        void trigger() noexcept;

        /** Test hook: reseeds the internal RNG so offline renders are deterministic. */
        void setSeed (juce::int64 seed) noexcept { random.setSeed (seed); }

        float processSample() noexcept;

    private:
        float renderBow() noexcept;
        float renderScrape() noexcept;
        float renderStrike() noexcept;
        float renderImpulse() noexcept;
        float renderPizzicato() noexcept;
        float renderTape() noexcept;
        float renderTapeLoop() noexcept;
        float renderVoice() noexcept;
        float renderBreath() noexcept;
        float renderNoise() noexcept;
        float renderSpark() noexcept;
        float renderSine() noexcept;
        float renderWavetable() noexcept;

        // Maps an excitation type to its wavetable shape index (WavetableBank::Shape).
        static int wavetableShapeFor (ExcitationType type) noexcept;

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
        double wtPhase      = 0.0;   // Wavetable cycle phase
        double loopPhase    = 0.0;   // TapeLoop worn-splice dropout cycle

        // Wavetable playback state.
        int   wtShape     = 0;       // WavetableBank::Shape index
        int   wtMip       = 0;       // harmonic-cap level for the current pitch
        float wtMorphBias = 0.5f;    // scan position (WAVE knob), default mid-table

        // Smoothed random walk state for Bow/Tape drift.
        float driftState   = 0.0f;
        float driftTarget  = 0.0f;
        int   driftCounter = 0;

        // Transient envelope shared by Strike/Spark.
        float transientEnv   = 0.0f;
        float transientCoeff = 0.0f;

        // Tape record-head rolloff (two cascaded one-poles, pre-saturation).
        float tapeTone1 = 0.0f, tapeTone2 = 0.0f;
        float tapeToneCoeff = 0.6f;

        juce::Random random;

        juce::dsp::StateVariableTPTFilter<float> noiseBand;
        juce::dsp::StateVariableTPTFilter<float> sparkBand;
        juce::dsp::StateVariableTPTFilter<float> formantLow;
        juce::dsp::StateVariableTPTFilter<float> formantHigh;
    };
}
