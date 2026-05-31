#include "PluginProcessor.h"

#include "PluginEditor.h"
#include "Core/MorphiumSound.h"
#include "Parameters/ParameterIDs.h"
#include "Parameters/ParameterLayout.h"
#include "Presets.h"

namespace morphium
{
MorphiumAudioProcessor::MorphiumAudioProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMETERS", createParameterLayout())
{
    wireVoiceParameters();

    synth.addSound (new MorphiumSound());
    for (int i = 0; i < numVoices; ++i)
        synth.addVoice (new MorphiumVoice (voiceParams));

    outputGainParam = apvts.getRawParameterValue (params::outputGain);
}

void MorphiumAudioProcessor::wireVoiceParameters()
{
    voiceParams.excitationType = apvts.getRawParameterValue (params::excitationType);
    voiceParams.density        = apvts.getRawParameterValue (params::density);
    voiceParams.mass           = apvts.getRawParameterValue (params::mass);
    voiceParams.friction       = apvts.getRawParameterValue (params::friction);
    voiceParams.wear           = apvts.getRawParameterValue (params::wear);
    voiceParams.attack         = apvts.getRawParameterValue (params::attack);
    voiceParams.decay          = apvts.getRawParameterValue (params::decay);
    voiceParams.sustain        = apvts.getRawParameterValue (params::sustain);
    voiceParams.release        = apvts.getRawParameterValue (params::release);
}

void MorphiumAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    synth.setCurrentPlaybackSampleRate (sampleRate);

    for (int i = 0; i < synth.getNumVoices(); ++i)
        if (auto* voice = dynamic_cast<MorphiumVoice*> (synth.getVoice (i)))
            voice->prepare (sampleRate, samplesPerBlock);

    outputGain.reset (sampleRate, 0.02);
}

bool MorphiumAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo();
}

void MorphiumAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;

    buffer.clear();

    synth.renderNextBlock (buffer, midi, 0, buffer.getNumSamples());

    outputGain.setTargetValue (juce::Decibels::decibelsToGain (outputGainParam->load()));

    const int numSamples  = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    for (int sample = 0; sample < numSamples; ++sample)
    {
        const float gain = outputGain.getNextValue();
        for (int channel = 0; channel < numChannels; ++channel)
            buffer.setSample (channel, sample, buffer.getSample (channel, sample) * gain);
    }
}

juce::AudioProcessorEditor* MorphiumAudioProcessor::createEditor()
{
    return new MorphiumAudioProcessorEditor (*this);
}

void MorphiumAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    if (! state.isValid())
        return;

    state.setProperty ("currentProgram", currentProgram, nullptr);

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void MorphiumAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto tree = juce::ValueTree::fromXml (*xml);
            currentProgram = (int) tree.getProperty ("currentProgram", 0);
            apvts.replaceState (tree);
        }
    }
}

// --- Programs / factory presets ---------------------------------------------

int MorphiumAudioProcessor::getNumPrograms()
{
    return (int) getFactoryPresets().size();
}

const juce::String MorphiumAudioProcessor::getProgramName (int index)
{
    const auto& presets = getFactoryPresets();
    if (juce::isPositiveAndBelow (index, (int) presets.size()))
        return presets[(size_t) index].name;
    return {};
}

void MorphiumAudioProcessor::setParameterValue (const char* id, float value)
{
    if (auto* p = apvts.getParameter (id))
        p->setValueNotifyingHost (p->convertTo0to1 (value));
}

void MorphiumAudioProcessor::setCurrentProgram (int index)
{
    const auto& presets = getFactoryPresets();
    if (! juce::isPositiveAndBelow (index, (int) presets.size()))
        return;

    currentProgram = index;
    const auto& p = presets[(size_t) index];

    setParameterValue (params::excitationType, (float) p.excitation);
    setParameterValue (params::density,  p.density);
    setParameterValue (params::mass,     p.mass);
    setParameterValue (params::friction, p.friction);
    setParameterValue (params::wear,     p.wear);
    setParameterValue (params::attack,   p.attack);
    setParameterValue (params::decay,    p.decay);
    setParameterValue (params::sustain,  p.sustain);
    setParameterValue (params::release,  p.release);
    setParameterValue (params::outputGain, p.outputGainDb);

    updateHostDisplay();
}
}

// This creates new instances of the plugin.
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new morphium::MorphiumAudioProcessor();
}
