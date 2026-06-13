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
    voiceParams.waveguideBlend = apvts.getRawParameterValue (params::waveguideBlend);
    voiceParams.wavePosition   = apvts.getRawParameterValue (params::wavePosition);
}

void MorphiumAudioProcessor::setDeterministicSeedForTests (juce::int64 baseSeed)
{
    for (int i = 0; i < synth.getNumVoices(); ++i)
        if (auto* voice = dynamic_cast<MorphiumVoice*> (synth.getVoice (i)))
            voice->setSeed (baseSeed + i);
}

int MorphiumAudioProcessor::countActiveVoicesForTests() const
{
    int count = 0;
    for (int i = 0; i < synth.getNumVoices(); ++i)
        if (synth.getVoice (i)->isVoiceActive())
            ++count;
    return count;
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
    spec.numChannels      = static_cast<juce::uint32> (juce::jmax (1, getTotalNumOutputChannels()));
    reverb.prepare (spec);

    driveOversampling.reset();
    driveOversampling.initProcessing (static_cast<size_t> (samplesPerBlock));

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

    // Panic: hard-stop every voice and flush the reverb before this block.
    if (panicRequested.exchange (false))
    {
        synth.allNotesOff (0, false);
        reverb.reset();
    }

    buffer.clear();

    synth.renderNextBlock (buffer, midi, 0, buffer.getNumSamples());

    // Headroom normalisation: full chords must not slam the reverb and drive.
    buffer.applyGain (1.0f / std::sqrt ((float) numVoices));

    // Apply Space Reverb prior to output gain
    if (reverbMixParam != nullptr && reverbSizeParam != nullptr)
    {
        // The matter colours its space: each material biases the reverb's
        // damping, and wear erodes the room's brightness further.
        static constexpr float materialDampingBias[] = {
            0.0f,   // OFF
            0.05f,  // METAL    — reverberant, bright tank
            0.10f,  // GLASS    — clear chamber
            0.40f,  // WOOD     — warm, absorptive room
            0.60f,  // TAPE     — muffled, magnetic
            0.20f,  // CIRCUIT  — electronic, medium
            0.30f,  // CONCRETE — dense, dark
        };
        const int material = juce::jlimit (0, 6,
            (int) voiceParams.resonatorMode->load());
        const float wear = voiceParams.wear->load();

        juce::dsp::Reverb::Parameters reverbParams;
        // Freeverb's comb feedback reaches 0.98 at roomSize 1.0, which lets a
        // sustained tone build up ~50x (+34 dB). Cap the room so the biggest
        // space stays bounded (~0.946 feedback, +25 dB worst case).
        reverbParams.roomSize = reverbSizeParam->load() * 0.88f;
        reverbParams.wetLevel = reverbMixParam->load() * 0.7f; // scale wet for perfect blend
        reverbParams.dryLevel = 1.0f - reverbParams.wetLevel;
        reverbParams.damping  = juce::jlimit (0.0f, 1.0f,
            0.25f + materialDampingBias[material] * wear);
        reverbParams.width    = 1.0f;
        reverb.setParameters (reverbParams);

        juce::dsp::AudioBlock<float> block (buffer);
        juce::dsp::ProcessContextReplacing<float> context (block);
        reverb.process (context);
    }

    outputGain.setTargetValue (juce::Decibels::decibelsToGain (outputGainParam->load()));
    outputGain.applyGain (buffer, buffer.getNumSamples());

    // Soft-clipping drive, processed at 2x to keep the tanh harmonics from
    // aliasing back into the audible band.
    const float driveAmt = driveParam != nullptr ? driveParam->load() : 0.0f;
    if (driveAmt > 0.01f)
    {
        const float preGain  = 1.0f + driveAmt * 10.0f;
        const float postGain = 1.0f / (1.0f + driveAmt * 2.0f);

        juce::dsp::AudioBlock<float> block (buffer);
        auto upBlock = driveOversampling.processSamplesUp (block);

        for (size_t channel = 0; channel < upBlock.getNumChannels(); ++channel)
        {
            float* data = upBlock.getChannelPointer (channel);
            for (size_t i = 0; i < upBlock.getNumSamples(); ++i)
                data[i] = std::tanh (data[i] * preGain) * postGain;
        }

        driveOversampling.processSamplesDown (block);
    }

    // Master safety clip: unity below the knee, then a smooth tanh shoulder
    // bounded at 1.0. Extreme patches (huge reverb + heavy friction drones)
    // can otherwise build up well past 0 dBFS.
    constexpr float knee = 0.85f;
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
    {
        float* data = buffer.getWritePointer (channel);
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            const float magnitude = std::abs (data[i]);
            if (magnitude > knee)
                data[i] = std::copysign (knee + (1.0f - knee)
                              * std::tanh ((magnitude - knee) / (1.0f - knee)),
                          data[i]);
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
