#include "MorphiumVoice.h"

#include "MorphiumSound.h"

namespace morphium
{
MorphiumVoice::MorphiumVoice (const VoiceParameters& sharedParameters)
    : params (sharedParameters)
{
}

void MorphiumVoice::prepare (double sampleRate, int /*samplesPerBlock*/)
{
    setCurrentPlaybackSampleRate (sampleRate);
    excitation.prepare (sampleRate);
    matter.prepare (sampleRate);
    adsr.setSampleRate (sampleRate);
    level.reset (sampleRate, 0.005);
    isPrepared = true;
}

bool MorphiumVoice::canPlaySound (juce::SynthesiserSound* sound)
{
    return dynamic_cast<MorphiumSound*> (sound) != nullptr;
}

void MorphiumVoice::updateParametersFromState() noexcept
{
    const auto typeIndex = juce::jlimit (0, static_cast<int> (ExcitationType::NumTypes) - 1,
                                         static_cast<int> (params.excitationType->load()));
    excitation.setType (static_cast<ExcitationType> (typeIndex));

    matter.setParameters (params.density->load(),
                          params.mass->load(),
                          params.friction->load(),
                          params.wear->load());

    adsrParams.attack  = params.attack->load();
    adsrParams.decay   = params.decay->load();
    adsrParams.sustain = params.sustain->load();
    adsrParams.release = params.release->load();
    adsr.setParameters (adsrParams);
}

void MorphiumVoice::startNote (int midiNoteNumber, float velocity,
                               juce::SynthesiserSound*, int /*pitchWheel*/)
{
    excitation.setFrequency (static_cast<float> (juce::MidiMessage::getMidiNoteInHertz (midiNoteNumber)));
    excitation.trigger();

    updateParametersFromState();
    adsr.noteOn();

    // Soft velocity curve, smoothed to avoid a click at the attack.
    level.setTargetValue (0.2f + 0.8f * velocity * velocity);
}

void MorphiumVoice::stopNote (float /*velocity*/, bool allowTailOff)
{
    if (allowTailOff)
    {
        adsr.noteOff();
    }
    else
    {
        adsr.reset();
        clearCurrentNote();
    }
}

void MorphiumVoice::pitchWheelMoved (int) {}
void MorphiumVoice::controllerMoved (int, int) {}

void MorphiumVoice::renderNextBlock (juce::AudioBuffer<float>& outputBuffer,
                                     int startSample, int numSamples)
{
    if (! isPrepared || ! isVoiceActive())
        return;

    updateParametersFromState();

    const int numChannels = outputBuffer.getNumChannels();

    for (int sample = 0; sample < numSamples; ++sample)
    {
        const float excited = excitation.processSample();
        const float shaped  = matter.processSample (excited);
        const float env     = adsr.getNextSample();
        const float out     = shaped * env * level.getNextValue();

        for (int channel = 0; channel < numChannels; ++channel)
            outputBuffer.addSample (channel, startSample + sample, out);

        if (! adsr.isActive())
        {
            clearCurrentNote();
            break;
        }
    }
}
}
