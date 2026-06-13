#include "MorphiumVoice.h"

#include "MorphiumSound.h"

namespace morphium
{
namespace
{
    float sympatheticIntervalForMaterial (int material) noexcept
    {
        switch (material)
        {
            case 2:  return 2.0f;        // GLASS: octave shimmer
            case 3:  return 4.0f / 3.0f; // WOOD: lower fourth body
            case 4:  return 1.01f;       // TAPE: near-unison beating
            case 5:  return 2.005f;      // CIRCUIT: stretched octave
            case 6:  return 1.85f;       // CONCRETE: clustered slab mode
            case 1:
            default: return 1.5f;        // METAL/OFF: perfect fifth
        }
    }
}

MorphiumVoice::MorphiumVoice (const VoiceParameters& sharedParameters)
    : params (sharedParameters)
{
}

void MorphiumVoice::prepare (double sampleRate, int /*samplesPerBlock*/)
{
    setCurrentPlaybackSampleRate (sampleRate);
    excitation.prepare (sampleRate);
    waveguide.prepare (sampleRate);
    sympatheticWaveguide.prepare (sampleRate);
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

    // The primary resonator's body follows the matter controls: mass darkens
    // the string's loop filter, wear deadens its ring, friction saturates it.
    const float mass = params.mass->load();
    const float wear = params.wear->load();
    waveguide.setDamping (mass * 0.6f, juce::jmap (wear, 6.0f, 0.8f));
    waveguide.setFriction (params.friction->load());

    const int material = juce::jlimit (0, BasicMatterProcessor::numMaterials,
                                       (int) params.resonatorMode->load());
    sympatheticWaveguide.setFrequency (currentFrequency * sympatheticIntervalForMaterial (material));
    sympatheticWaveguide.setDamping (juce::jlimit (0.03f, 0.45f, mass * 0.35f + 0.03f),
                                     juce::jmap (wear, 2.8f, 0.35f));
    sympatheticWaveguide.setFriction (params.friction->load() * 0.65f);

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

    waveguide.reset(); // a fresh string, retuned to the new note
    waveguide.setFrequency (currentFrequency);
    sympatheticWaveguide.reset();
    sympatheticWaveguide.setFrequency (currentFrequency * 1.5f);
    sympatheticKick = velocity * 0.35f;

    lfoPhase = 0.0f; // reset LFO phase on keypress for consistent transient feel
    controlCounter = 0;
    silentSamples = 0;
    updateParametersFromState();
    adsr.noteOn();

    // Soft velocity curve, smoothed to avoid a click at the attack.
    level.setTargetValue (0.15f + 0.35f * velocity * velocity);
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
    const float rateHz       = params.lfoRate->load();
    const float depth        = params.lfoDepth->load();
    const float baseDensity  = params.density->load();
    const float baseMass     = params.mass->load();
    const float baseFriction = params.friction->load();
    const float baseWear     = params.wear->load();
    const float wgBlend      = params.waveguideBlend->load();
    const float waveBase     = params.wavePosition->load();
    const double sr = getSampleRate();

    for (int sample = 0; sample < numSamples; ++sample)
    {
        // Control-rate LFO: retarget the matter smoothers every 64 samples and
        // let their 20 ms ramps interpolate — per-sample retargeting would
        // bypass the smoothing entirely.
        if (controlCounter <= 0)
        {
            controlCounter = controlRatePeriod;

            const float lfoVal = std::sin (lfoPhase);
            lfoPhase += juce::MathConstants<float>::twoPi * rateHz
                        * (float) controlRatePeriod / static_cast<float> (sr);
            if (lfoPhase >= juce::MathConstants<float>::twoPi)
                lfoPhase -= juce::MathConstants<float>::twoPi;

            matter.setParameters (baseDensity + lfoVal * depth * 0.22f,
                                  baseMass    + lfoVal * depth * 0.18f,
                                  baseFriction, baseWear);

            // LFO on the string's loop gain: bow-pressure tremolo on the body
            // itself rather than EQ wobble.
            waveguide.setGainTrim (1.0f + lfoVal * depth * 0.02f);

            // Wavetable scan: the WAVE knob sets the centre, the Motion LFO
            // sweeps the morph frames around it (ignored by non-wavetable sources).
            excitation.setMorphPosition (waveBase + lfoVal * depth * 0.5f);
        }
        --controlCounter;

        // The ADSR shapes the *audible output* (excitation -> resonator -> VCA):
        // ATTACK/DECAY/SUSTAIN/RELEASE do exactly what the knobs promise, and a
        // released note can't ring past its release. The resonator still adds
        // its physical body and a natural ring *within* the envelope.
        const float env     = adsr.getNextSample();
        const float excited = excitation.processSample() * level.getNextValue();

        // Primary resonator: the excitation drives the waveguide string and
        // the blend crossfades the legacy oscillator path into the physical
        // model (0 keeps old presets byte-compatible).
        float voiceSignal = excited;
        float sympatheticOut = 0.0f;
        if (wgBlend > 0.0001f)
        {
            const float stringOut = waveguide.processSample (excited);
            const float sympatheticDrive = excited * 0.18f + stringOut * 0.03f + sympatheticKick;
            sympatheticKick = 0.0f;
            sympatheticOut = sympatheticWaveguide.processSample (sympatheticDrive);
            const float physicalOut = stringOut + sympatheticOut * 0.18f;
            voiceSignal = excited + wgBlend * (physicalOut - excited);
        }
        else
        {
            sympatheticKick = 0.0f;
        }

        float shaped = matter.processSample (voiceSignal);
        shaped += sympatheticOut * wgBlend * 0.20f;

        // VCA: the envelope gates the final voice so release/sustain are audible
        // and the voice cannot outlive its release with an immortal resonator tail.
        shaped *= env;

        for (int channel = 0; channel < numChannels; ++channel)
            outputBuffer.addSample (channel, startSample + sample, shaped);

        if (! adsr.isActive())
        {
            // Free the voice once the resonator ring has actually died away.
            if (std::abs (shaped) < 1.0e-5f)
            {
                if (++silentSamples >= 1024)
                {
                    clearCurrentNote();
                    break;
                }
            }
            else
            {
                silentSamples = 0;
            }
        }
    }
}
}
