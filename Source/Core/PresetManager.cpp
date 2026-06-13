#include "PresetManager.h"
#include "../Parameters/ParameterIDs.h"
#include <juce_data_structures/juce_data_structures.h>

namespace morphium
{

PresetManager::PresetManager(juce::AudioProcessorValueTreeState& apvts)
    : valueTreeState(apvts)
{
    auto appData = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory);
    defaultDirectory = appData.getChildFile("BoratoAudio").getChildFile("BoratoMorphium").getChildFile("Presets");
    if (!defaultDirectory.exists())
        defaultDirectory.createDirectory();
}

juce::String PresetManager::getCurrentPresetName() const
{
    if (currentFactoryIndex >= 0 && currentFactoryIndex < (int)getFactoryPresets().size())
        return getFactoryPresets()[(size_t)currentFactoryIndex].name;
    
    if (currentUserPresetName.isNotEmpty())
        return currentUserPresetName;
        
    return "INIT DEFAULT";
}

void PresetManager::loadFactoryPreset(int index)
{
    const auto& presets = getFactoryPresets();
    if (index < 0 || index >= (int)presets.size()) return;
    
    const auto& p = presets[(size_t)index];
    currentFactoryIndex = index;
    currentUserPresetName = "";
    
    auto setParam = [&](const juce::String& id, float val) {
        if (auto* param = valueTreeState.getParameter(id))
            param->setValueNotifyingHost(param->convertTo0to1(val));
    };

    setParam(params::excitationType, (float)p.excitation);
    setParam(params::density, p.density);
    setParam(params::mass, p.mass);
    setParam(params::friction, p.friction);
    setParam(params::wear, p.wear);
    setParam(params::attack, p.attack);
    setParam(params::decay, p.decay);
    setParam(params::sustain, p.sustain);
    setParam(params::release, p.release);
    setParam(params::lfoRate, p.lfoRate);
    setParam(params::lfoDepth, p.lfoDepth);
    setParam(params::reverbSize, p.reverbSize);
    setParam(params::reverbMix, p.reverbMix);
    setParam(params::resonatorMode, (float)p.resonatorMode);
    setParam(params::waveguideBlend, p.waveguide);
    setParam(params::wavePosition, p.wavePos);
}

void PresetManager::saveUserPreset(const juce::String& name)
{
    auto file = defaultDirectory.getChildFile(name + ".xml");
    auto state = valueTreeState.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    if (xml != nullptr)
    {
        xml->writeTo(file);
        currentUserPresetName = name;
        currentFactoryIndex = -1;
    }
}

void PresetManager::loadUserPreset(const juce::String& name)
{
    auto file = defaultDirectory.getChildFile(name + ".xml");
    if (!file.existsAsFile()) return;
    
    if (auto xml = juce::XmlDocument::parse(file))
    {
        valueTreeState.replaceState(juce::ValueTree::fromXml(*xml));
        currentUserPresetName = name;
        currentFactoryIndex = -1;
    }
}

juce::StringArray PresetManager::getUserPresets() const
{
    juce::StringArray presets;
    auto files = defaultDirectory.findChildFiles(juce::File::findFiles, false, "*.xml");
    for (const auto& f : files)
        presets.add(f.getFileNameWithoutExtension());
    return presets;
}

void PresetManager::deleteUserPreset(const juce::String& name)
{
    auto file = defaultDirectory.getChildFile(name + ".xml");
    if (file.existsAsFile())
        file.deleteFile();
}

}
