#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>

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
        double getTailLengthSeconds() const override { return 0.0; }

        int getNumPrograms() override;
        int getCurrentProgram() override { return currentProgram; }
        void setCurrentProgram (int index) override;
        const juce::String getProgramName (int index) override;
        void changeProgramName (int, const juce::String&) override {}

        void getStateInformation (juce::MemoryBlock& destData) override;
        void setStateInformation (const void* data, int sizeInBytes) override;

        juce::AudioProcessorValueTreeState& getValueTreeState() { return apvts; }
        morphium::PresetManager presetManager;

    private:
        static constexpr int numVoices = 8;

        juce::AudioProcessorValueTreeState apvts;
        VoiceParameters voiceParams;
        juce::Synthesiser synth;

        juce::SmoothedValue<float> outputGain;
        std::atomic<float>* outputGainParam = nullptr;
        std::atomic<float>* reverbSizeParam = nullptr;
        std::atomic<float>* reverbMixParam  = nullptr;

        juce::dsp::Reverb reverb;

        int currentProgram = 0;

        void wireVoiceParameters();
        void setParameterValue (const char* id, float value);

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MorphiumAudioProcessor)
    };
}
