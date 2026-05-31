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

    // Initially set base parameters
    matter.setParameters (params.density->load(),
                          params.mass->load(),
                          params.friction->load(),
                          params.wear->load());
    matter.setResonatorParameters (static_cast<int> (params.resonatorMode->load()), currentFrequency);

    juce::ADSR::Parameters newAdsr;
    newAdsr.attack  = params.attack->load();
    newAdsr.decay   = params.decay->load();
    newAdsr.sustain = params.sustain->load();
    newAdsr.release = params.release->load();

    if (adsrParams.attack != newAdsr.attack ||
        adsrParams.decay != newAdsr.decay ||
        adsrParams.sustain != newAdsr.sustain ||
        adsrParams.release != newAdsr.release)
    {
        adsrParams = newAdsr;
        adsr.setParameters (adsrParams);
    }
}

void MorphiumVoice::startNote (int midiNoteNumber, float velocity,
                               juce::SynthesiserSound*, int /*pitchWheel*/)
{
    currentFrequency = static_cast<float> (juce::MidiMessage::getMidiNoteInHertz (midiNoteNumber));
    excitation.setFrequency (currentFrequency);
    excitation.trigger();

    lfoPhase = 0.0f; // reset LFO phase on keypress for consistent transient feel
    updateParametersFromState();
    adsr.noteOn();

    // Soft velocity curve, smoothed to avoid a click at the attack.
    level.setTargetValue (0.05f + 0.25f * velocity * velocity);
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
    const float rateHz = params.lfoRate->load();
    const float depth = params.lfoDepth->load();
    const double sr = getSampleRate();

    for (int sample = 0; sample < numSamples; ++sample)
    {
        // Advance polyphonic LFO
        const float lfoVal = std::sin (lfoPhase);
        lfoPhase += juce::MathConstants<float>::twoPi * rateHz / static_cast<float> (sr);
        if (lfoPhase >= juce::MathConstants<float>::twoPi)
            lfoPhase -= juce::MathConstants<float>::twoPi;

        // Apply LFO drift to Matter shaping parameters
        const float densityMod = params.density->load() + lfoVal * depth * 0.22f;
        const float massMod = params.mass->load() + lfoVal * depth * 0.18f;
        matter.setParameters (densityMod, massMod, params.friction->load(), params.wear->load());

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
