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
      apvts (*this, nullptr, "PARAMETERS", createParameterLayout()),
      presetManager (apvts)
{
    wireVoiceParameters();

    synth.addSound (new MorphiumSound());
    for (int i = 0; i < numVoices; ++i)
        synth.addVoice (new MorphiumVoice (voiceParams));

    outputGainParam = apvts.getRawParameterValue (params::outputGain);
    reverbSizeParam = apvts.getRawParameterValue (params::reverbSize);
    reverbMixParam  = apvts.getRawParameterValue (params::reverbMix);
    driveParam      = apvts.getRawParameterValue (params::drive);
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
    voiceParams.lfoRate        = apvts.getRawParameterValue (params::lfoRate);
    voiceParams.lfoDepth       = apvts.getRawParameterValue (params::lfoDepth);
    voiceParams.resonatorMode  = apvts.getRawParameterValue (params::resonatorMode);
}

void MorphiumAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    synth.setCurrentPlaybackSampleRate (sampleRate);

    for (int i = 0; i < synth.getNumVoices(); ++i)
        if (auto* voice = dynamic_cast<MorphiumVoice*> (synth.getVoice (i)))
            voice->prepare (sampleRate, samplesPerBlock);

    juce::dsp::ProcessSpec spec;
    spec.sampleRate       = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32> (samplesPerBlock);
    spec.numChannels      = 2;
    reverb.prepare (spec);

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

    // Apply Space Reverb prior to output gain
    if (reverbMixParam != nullptr && reverbSizeParam != nullptr)
    {
        juce::dsp::Reverb::Parameters reverbParams;
        reverbParams.roomSize = reverbSizeParam->load();
        reverbParams.wetLevel = reverbMixParam->load() * 0.7f; // scale wet for perfect blend
        reverbParams.dryLevel = 1.0f - reverbParams.wetLevel;
        reverbParams.damping  = 0.25f;
        reverbParams.width    = 1.0f;
        reverb.setParameters (reverbParams);

        juce::dsp::AudioBlock<float> block (buffer);
        juce::dsp::ProcessContextReplacing<float> context (block);
        reverb.process (context);
    }

    outputGain.setTargetValue (juce::Decibels::decibelsToGain (outputGainParam->load()));

    const int numSamples  = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    float driveAmt = driveParam != nullptr ? driveParam->load() : 0.0f;
    float preGain = 1.0f + driveAmt * 10.0f;
    float postGain = 1.0f / (1.0f + driveAmt * 2.0f);

    for (int sample = 0; sample < numSamples; ++sample)
    {
        const float gain = outputGain.getNextValue();
        for (int channel = 0; channel < numChannels; ++channel)
        {
            float s = buffer.getSample (channel, sample) * gain;
            
            // Soft clipping drive
            if (driveAmt > 0.01f)
                s = std::tanh (s * preGain) * postGain;

            buffer.setSample (channel, sample, s);
        }
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

    state.setProperty ("currentFactoryIndex", presetManager.currentFactoryIndex, nullptr);
    state.setProperty ("currentUserPresetName", presetManager.currentUserPresetName, nullptr);

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
            presetManager.currentFactoryIndex = (int) tree.getProperty ("currentFactoryIndex", 0);
            presetManager.currentUserPresetName = tree.getProperty ("currentUserPresetName", "").toString();
            currentProgram = presetManager.currentFactoryIndex;
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
    presetManager.loadFactoryPreset(index);
    currentProgram = presetManager.currentFactoryIndex;

    updateHostDisplay();
}
}

// This creates new instances of the plugin.
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new morphium::MorphiumAudioProcessor();
}
