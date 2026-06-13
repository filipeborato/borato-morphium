#pragma once

#include <juce_audio_utils/juce_audio_utils.h>

#include "ExcitationCore.h"
#include "../DSP/BasicMatterProcessor.h"
#include "../DSP/WaveguideResonator.h"

namespace morphium
{
    /**
        Atomic snapshot of the parameters a voice needs to read on the audio
        thread. The processor wires these pointers to the APVTS raw values once;
        each voice loads them per block. This is allocation- and lock-free.
    */
    struct VoiceParameters
    {
        std::atomic<float>* excitationType = nullptr;
        std::atomic<float>* density        = nullptr;
        std::atomic<float>* mass           = nullptr;
        std::atomic<float>* friction       = nullptr;
        std::atomic<float>* wear           = nullptr;
        std::atomic<float>* attack         = nullptr;
        std::atomic<float>* decay          = nullptr;
        std::atomic<float>* sustain        = nullptr;
        std::atomic<float>* release        = nullptr;
        std::atomic<float>* lfoRate        = nullptr;
        std::atomic<float>* lfoDepth       = nullptr;
        std::atomic<float>* resonatorMode  = nullptr;
        std::atomic<float>* waveguideBlend = nullptr;
        std::atomic<float>* wavePosition   = nullptr;
    };

    /**
        One polyphonic voice:
            note -> ExcitationCore -> BasicMatterProcessor (with Resonator) -> ADSR -> mix
    */
    class MorphiumVoice : public juce::SynthesiserVoice
    {
    public:
        explicit MorphiumVoice (const VoiceParameters& sharedParameters);

        void prepare (double sampleRate, int samplesPerBlock);

        bool canPlaySound (juce::SynthesiserSound* sound) override;

        void startNote (int midiNoteNumber, float velocity,
                        juce::SynthesiserSound* sound, int currentPitchWheelPosition) override;
        void stopNote (float velocity, bool allowTailOff) override;

        void pitchWheelMoved (int newPitchWheelValue) override;
        void controllerMoved (int controllerNumber, int newControllerValue) override;

        void renderNextBlock (juce::AudioBuffer<float>& outputBuffer,
                              int startSample, int numSamples) override;

        /** Test hook: makes the voice's random behaviour deterministic. */
        void setSeed (juce::int64 seed) noexcept
        {
            excitation.setSeed (seed);
            matter.setSeed (seed + 0x9E3779B9);
        }

    private:
        void updateParametersFromState() noexcept;

        const VoiceParameters& params;

        ExcitationCore       excitation;
        WaveguideResonator   waveguide;
        WaveguideResonator   sympatheticWaveguide;
        BasicMatterProcessor matter;
        juce::ADSR           adsr;
        juce::ADSR::Parameters adsrParams;

        juce::SmoothedValue<float> level;   // velocity, smoothed to avoid clicks
        bool isPrepared = false;

        // LFO targets are recomputed at control rate so the matter stage's
        // 20 ms parameter smoothers do the per-sample interpolation.
        static constexpr int controlRatePeriod = 64;
        int controlCounter = 0;

        // After the release ends the resonators keep ringing; the voice is
        // freed once the output has stayed silent for this many samples.
        int silentSamples = 0;

        float sympatheticKick = 0.0f;
        float lfoPhase = 0.0f;
        float currentFrequency = 440.0f;
    };
}
