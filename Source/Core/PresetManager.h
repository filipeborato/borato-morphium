#pragma once

#include <juce_core/juce_core.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "../Presets.h"

namespace morphium
{
    class PresetManager
    {
    public:
        PresetManager(juce::AudioProcessorValueTreeState& apvts);

        juce::String getCurrentPresetName() const;
        
        void loadFactoryPreset(int index);
        void saveUserPreset(const juce::String& name);
        void loadUserPreset(const juce::String& name);
        
        juce::StringArray getUserPresets() const;
        void deleteUserPreset(const juce::String& name);
        void renameUserPreset(const juce::String& oldName, const juce::String& newName);

        int currentFactoryIndex = 0;
        juce::String currentUserPresetName;

    private:
        juce::AudioProcessorValueTreeState& valueTreeState;
        juce::File defaultDirectory;
    };
}
