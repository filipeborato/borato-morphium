#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>

#include <array>
#include <atomic>

#include "Core/MorphiumVoice.h"
#include "Core/PresetManager.h"

namespace morphium
{
    class MorphiumAudioProcessor : public juce::AudioProcessor
    {
    public:
        MorphiumAudioProcessor();
        ~MorphiumAudioProcessor() override = default;

        void prepareToPlay (double sampleRate, int samplesPerBlock) override;
        void releaseResources() override {}
        bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
        void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

        juce::AudioProcessorEditor* createEditor() override;
        bool hasEditor() const override { return true; }

        const juce::String getName() const override { return "Borato Morphium"; }

        bool acceptsMidi() const override  { return true; }
        bool producesMidi() const override { return false; }
        bool isMidiEffect() const override { return false; }
        double getTailLengthSeconds() const override { return 8.0; }

        int getNumPrograms() override;
        int getCurrentProgram() override { return currentProgram; }
        void setCurrentProgram (int index) override;
        const juce::String getProgramName (int index) override;
        void changeProgramName (int, const juce::String&) override {}

        void getStateInformation (juce::MemoryBlock& destData) override;
        void setStateInformation (const void* data, int sizeInBytes) override;

        juce::AudioProcessorValueTreeState& getValueTreeState() { return apvts; }
        morphium::PresetManager presetManager;

        // Scope buffer: the audio thread writes the latest output samples here;
        // the editor reads them at 30 Hz for the oscilloscope display.
        static constexpr int scopeBufferSize = 512;
        std::array<float, scopeBufferSize> scopeBuffer {};
        std::atomic<int> scopeWritePos { 0 };

        // Peak levels for the output meter (updated per processBlock).
        std::atomic<float> peakLeft { 0.0f };
        std::atomic<float> peakRight { 0.0f };

        /** Test hook: seeds every voice's RNG so offline renders are reproducible. */
        void setDeterministicSeedForTests (juce::int64 baseSeed);

        /** Test hook: number of voices currently sounding a note. */
        int countActiveVoicesForTests() const;

        /** Panic: immediately silence every voice and clear the reverb tail.
            Safe to call from the message thread — the request is serviced at the
            top of the next processBlock. Clears notes left stuck by a lost
            note-off (e.g. a Standalone keyboard key released off-focus). */
        void panic() noexcept { panicRequested.store (true); }

    private:
        static constexpr int numVoices = 8;

        juce::AudioProcessorValueTreeState apvts;
        VoiceParameters voiceParams;
        juce::Synthesiser synth;

        juce::SmoothedValue<float> outputGain;
        std::atomic<float>* outputGainParam = nullptr;
        std::atomic<float>* reverbSizeParam = nullptr;
        std::atomic<float>* reverbMixParam  = nullptr;
        std::atomic<float>* driveParam      = nullptr;

        juce::dsp::Reverb reverb;

        // Set from the message thread (panic()); serviced on the audio thread.
        std::atomic<bool> panicRequested { false };

        // 2x oversampling for the output drive stage: tanh at 44.1 kHz folds
        // its upper harmonics back as inharmonic junk; at 2x they land above
        // the audible band and are filtered out on the way down.
        juce::dsp::Oversampling<float> driveOversampling {
            2, 1, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true
        };

        int currentProgram = 0;

        void wireVoiceParameters();
        void setParameterValue (const char* id, float value);

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MorphiumAudioProcessor)
    };
}
