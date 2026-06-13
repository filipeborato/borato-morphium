#include <catch2/catch_test_macros.hpp>

#include "SpectralUtils.h"
#include "PluginProcessor.h"
#include "Parameters/ParameterIDs.h"
#include "Core/ExcitationCore.h"

using namespace morphium;
using namespace morphium::test;

namespace
{
    void setParam (MorphiumAudioProcessor& processor, const char* id, float value)
    {
        auto* parameter = processor.getValueTreeState().getParameter (id);
        REQUIRE (parameter != nullptr);
        parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
    }

    constexpr int sr = 44100;
}

TEST_CASE ("Voice: released notes decay to silence (no stuck notes)", "[voice][adsr]")
{
    MorphiumAudioProcessor processor;
    processor.setDeterministicSeedForTests (12345);

    // 1 s note, then 4 s of post-roll. Default release is 0.4 s and default
    // reverb mix is modest, so the last second must be silent.
    const auto buffer = renderSingleNote (processor, 69, 0.8f, sr, 5 * sr);

    REQUIRE (! containsNonFinite (buffer));
    const float tailDb = rmsDb (buffer, 4 * sr, sr);
    INFO ("tail RMS = " << tailDb << " dBFS");
    CHECK (tailDb < -60.0f);
}

// Regression guard for the "grotesque" stuck note: turning knobs (the WAVE
// scan, the material, density/mass, the waveguide blend) while a note plays and
// after it is released must never leave a voice ringing forever. A released note
// has to decay to silence regardless of how violently parameters are automated.
TEST_CASE ("Voice: parameter automation never leaves a stuck note", "[voice][stuck][automation]")
{
    constexpr int blockSize = 64;

    for (int program : { 0, 18, 37 })   // default, a waveguide patch, a wavetable patch
    {
        MorphiumAudioProcessor processor;
        processor.setCurrentProgram (program);
        processor.setDeterministicSeedForTests (12345);
        processor.prepareToPlay ((double) sr, blockSize);

        auto& apvts = processor.getValueTreeState();
        auto* pWave = apvts.getParameter (params::wavePosition);
        auto* pRes  = apvts.getParameter (params::resonatorMode);
        auto* pDens = apvts.getParameter (params::density);
        auto* pMass = apvts.getParameter (params::mass);
        auto* pWg   = apvts.getParameter (params::waveguideBlend);

        juce::AudioBuffer<float> block (2, blockSize);
        bool finite = true;
        float phase = 0.0f;

        auto run = [&] (int numBlocks, bool automate) -> float
        {
            float maxAbs = 0.0f;
            for (int b = 0; b < numBlocks; ++b)
            {
                if (automate)
                {
                    phase += 0.17f;
                    const float s = 0.5f + 0.5f * std::sin (phase);
                    pWave->setValueNotifyingHost (s);
                    pDens->setValueNotifyingHost (1.0f - s);
                    pMass->setValueNotifyingHost (s);
                    pWg->setValueNotifyingHost (s);
                    pRes->setValueNotifyingHost (0.5f + 0.5f * std::sin (phase * 0.37f));
                }

                juce::MidiBuffer midi;
                block.clear();
                processor.processBlock (block, midi);

                finite = finite && ! containsNonFinite (block);
                maxAbs = juce::jmax (maxAbs, block.getMagnitude (0, 0, blockSize),
                                             block.getMagnitude (1, 0, blockSize));
            }
            return maxAbs;
        };

        // Note on, then hold ~0.5 s while sweeping every knob hard.
        {
            juce::MidiBuffer midi;
            midi.addEvent (juce::MidiMessage::noteOn (1, 69, 0.9f), 0);
            block.clear();
            processor.processBlock (block, midi);
        }
        const float held = run ((int) (0.5 * sr) / blockSize, true);

        // Note off; keep sweeping briefly, then let it settle for a long time
        // (longer than the longest release + reverb tail).
        {
            juce::MidiBuffer midi;
            midi.addEvent (juce::MidiMessage::noteOff (1, 69), 0);
            block.clear();
            processor.processBlock (block, midi);
        }
        run ((int) (0.5 * sr) / blockSize, true);
        run ((int) (10.0 * sr) / blockSize, false);

        // The TRUE tail: the peak over the final second, after everything has
        // had time to die. A stuck note rings here; a healthy synth is silent.
        const float tail = run ((int) (1.0 * sr) / blockSize, false);

        const int activeVoices = processor.countActiveVoicesForTests();
        INFO ("program " << program << " held peak " << held
              << ", final-second peak " << tail
              << ", active voices = " << activeVoices);
        CHECK (finite);
        CHECK (activeVoices == 0);          // every released voice must free
        CHECK (tail < 0.003f);              // ~ -50 dBFS: effectively silent
    }
}

// Panic must clear notes that were never released (the real-world stuck note:
// a lost note-off). A held note that is never released would otherwise sustain
// forever; panic() has to silence it immediately.
TEST_CASE ("Voice: panic silences notes left held without a note-off", "[voice][panic]")
{
    constexpr int blockSize = 64;

    MorphiumAudioProcessor processor;
    processor.setCurrentProgram (37);   // a sustaining wavetable pad
    processor.setDeterministicSeedForTests (12345);
    processor.prepareToPlay ((double) sr, blockSize);

    juce::AudioBuffer<float> block (2, blockSize);

    auto renderTo = [&] (int numBlocks) -> float
    {
        float maxAbs = 0.0f;
        for (int b = 0; b < numBlocks; ++b)
        {
            juce::MidiBuffer midi;
            block.clear();
            processor.processBlock (block, midi);
            maxAbs = juce::jmax (maxAbs, block.getMagnitude (0, 0, blockSize));
        }
        return maxAbs;
    };

    // Note ON, then keep rendering WITHOUT ever sending a note-off (the stuck
    // condition). The voice must still be sounding and active.
    {
        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::noteOn (1, 69, 0.9f), 0);
        block.clear();
        processor.processBlock (block, midi);
    }
    const float held = renderTo ((int) (1.0 * sr) / blockSize);
    REQUIRE (held > 0.05f);
    REQUIRE (processor.countActiveVoicesForTests() > 0);

    // PANIC.
    processor.panic();

    // After a short flush the synth must be silent and every voice freed.
    const float tail = renderTo ((int) (0.5 * sr) / blockSize);
    INFO ("held " << held << ", post-panic peak " << tail
          << ", active voices " << processor.countActiveVoicesForTests());
    CHECK (processor.countActiveVoicesForTests() == 0);
    CHECK (tail < 0.003f);
}

TEST_CASE ("Voice: 8-voice fortissimo stays bounded for every source", "[voice][gain]")
{
    const int notes[] = { 48, 52, 55, 57, 60, 64, 67, 69 };

    for (int typeIndex = 0; typeIndex < (int) ExcitationType::NumTypes; ++typeIndex)
    {
        MorphiumAudioProcessor processor;
        processor.setDeterministicSeedForTests (12345);
        setParam (processor, params::excitationType, (float) typeIndex);

        std::vector<MidiEvent> events;
        for (int note : notes)
        {
            events.push_back ({ 0,  juce::MidiMessage::noteOn  (1, note, 1.0f) });
            events.push_back ({ sr, juce::MidiMessage::noteOff (1, note) });
        }

        const auto buffer = renderProcessor (processor, events, sr, 44100.0);

        INFO ("excitation type " << typeIndex);
        REQUIRE (! containsNonFinite (buffer));
        // 1/sqrt(numVoices) bus normalisation + master safety clip: even an
        // 8-voice fortissimo stays at or below 0 dBFS.
        CHECK (peakLinear (buffer) < 1.01f);
    }
}

TEST_CASE ("Voice: output carries no meaningful DC offset", "[voice][dc]")
{
    for (int typeIndex = 0; typeIndex < (int) ExcitationType::NumTypes; ++typeIndex)
    {
        MorphiumAudioProcessor processor;
        processor.setDeterministicSeedForTests (12345);
        setParam (processor, params::excitationType, (float) typeIndex);

        const auto buffer = renderSingleNote (processor, 69, 0.8f, sr, sr);

        INFO ("excitation type " << typeIndex);
        CHECK (std::abs (dcOffset (buffer)) < 0.005f);
    }
}

TEST_CASE ("Voice: LFO modulation shows up at the LFO rate", "[voice][lfo]")
{
    MorphiumAudioProcessor processor;
    processor.setDeterministicSeedForTests (12345);
    setParam (processor, params::excitationType, (float) ExcitationType::Bow);
    setParam (processor, params::lfoRate, 10.0f);
    setParam (processor, params::lfoDepth, 1.0f);
    setParam (processor, params::sustain, 1.0f);
    setParam (processor, params::reverbMix, 0.0f);

    const auto buffer = renderSingleNote (processor, 69, 0.8f, 3 * sr, 3 * sr);
    REQUIRE (! containsNonFinite (buffer));

    // RMS envelope at hop 128 (-> 344.5 Hz envelope rate), skipping the attack.
    constexpr int hop = 128;
    const float* data = buffer.getReadPointer (0);
    std::vector<float> envelope;
    for (int pos = sr / 2; pos + hop <= buffer.getNumSamples(); pos += hop)
    {
        double sum = 0.0;
        for (int i = 0; i < hop; ++i)
            sum += (double) data[pos + i] * data[pos + i];
        envelope.push_back ((float) std::sqrt (sum / hop));
    }

    float mean = 0.0f;
    for (float e : envelope)
        mean += e;
    mean /= (float) envelope.size();
    for (float& e : envelope)
        e -= mean;

    const double envelopeRate = (double) sr / hop;
    const auto spectrum = analyse (envelope.data(), (int) envelope.size(), envelopeRate, 10);

    const float modulationHz = detectF0 (spectrum, 3.0f, 18.0f);
    INFO ("strongest envelope modulation at " << modulationHz << " Hz (LFO set to 10 Hz)");
    CHECK (modulationHz > 8.0f);
    CHECK (modulationHz < 12.0f);
}
