#include "ExcitationCore.h"

#include "../DSP/PolyBlep.h"
#include "../DSP/Wavetable.h"

namespace morphium
{
namespace
{
    constexpr float twoPi = juce::MathConstants<float>::twoPi;

    inline float fastSine (double normalisedPhase) noexcept
    {
        return std::sin (static_cast<float> (normalisedPhase) * twoPi);
    }

    // Keeps a filter cutoff inside the stable range for the current sample rate.
    inline float clampCutoff (float hz, double sampleRate) noexcept
    {
        const auto nyquist = static_cast<float> (sampleRate * 0.49);
        return juce::jlimit (20.0f, nyquist, hz);
    }
}

juce::StringArray getExcitationTypeNames()
{
    return {
        "STRIKE", "IMPULSE", "PIZZICATO",
        "BOW", "SCRAPE",
        "VOICE", "BREATH",
        "SPARK", "SINE", "WT HARMONIC",
        "NOISE",
        "TAPE", "TAPE LOOP",
        "WT PWM", "WT FORMANT", "WT METALLIC", "WT EPIANO", "WT DIGITAL"
    };
}

bool isWavetableType (ExcitationType t) noexcept
{
    switch (t)
    {
        case ExcitationType::Wavetable:
        case ExcitationType::WtPwm:
        case ExcitationType::WtFormant:
        case ExcitationType::WtMetallic:
        case ExcitationType::WtEPiano:
        case ExcitationType::WtDigital: return true;
        default:                        return false;
    }
}

int ExcitationCore::wavetableShapeFor (ExcitationType t) noexcept
{
    switch (t)
    {
        case ExcitationType::Wavetable:  return (int) WavetableBank::Shape::Harmonic;
        case ExcitationType::WtPwm:      return (int) WavetableBank::Shape::Pwm;
        case ExcitationType::WtFormant:  return (int) WavetableBank::Shape::Formant;
        case ExcitationType::WtMetallic: return (int) WavetableBank::Shape::Metallic;
        case ExcitationType::WtEPiano:   return (int) WavetableBank::Shape::EPiano;
        case ExcitationType::WtDigital:  return (int) WavetableBank::Shape::Digital;
        default:                         return (int) WavetableBank::Shape::Harmonic;
    }
}

ExcitationCore::ExcitationCore() = default;

void ExcitationCore::prepare (double newSampleRate)
{
    sampleRate = newSampleRate;

    juce::dsp::ProcessSpec spec;
    spec.sampleRate       = sampleRate;
    spec.maximumBlockSize = 512;       // per-sample processing; value is a hint
    spec.numChannels      = 1;

    for (auto* filter : { &noiseBand, &sparkBand, &formantLow, &formantHigh })
    {
        filter->prepare (spec);
        filter->setType (juce::dsp::StateVariableTPTFilterType::bandpass);
    }

    formantLow.setResonance  (3.0f);
    formantHigh.setResonance (4.0f);
    noiseBand.setResonance   (1.5f);
    sparkBand.setResonance   (6.0f);

    tapeToneCoeff = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * 7000.0f
                                     / static_cast<float> (sampleRate));

    setFrequency (frequency);
    reset();
}

void ExcitationCore::reset()
{
    mainPhase = wowPhase = flutterPhase = wtPhase = loopPhase = 0.0;
    driftState = driftTarget = 0.0f;
    driftCounter = 0;
    transientEnv = 0.0f;
    tapeTone1 = tapeTone2 = 0.0f;

    for (auto* filter : { &noiseBand, &sparkBand, &formantLow, &formantHigh })
        filter->reset();
}

void ExcitationCore::setFrequency (float frequencyHz) noexcept
{
    frequency = frequencyHz;
    phaseInc  = frequency / sampleRate;

    // Note-dependent filter tunings.
    noiseBand.setCutoffFrequency  (clampCutoff (frequency * 2.0f, sampleRate));
    sparkBand.setCutoffFrequency  (clampCutoff (frequency * 6.0f, sampleRate));

    // Vowel-like static formants ("ah"), but tracked up high so notes stay lively.
    formantLow.setCutoffFrequency  (clampCutoff (juce::jmax (700.0f, frequency * 1.0f), sampleRate));
    formantHigh.setCutoffFrequency (clampCutoff (juce::jmax (1100.0f, frequency * 2.0f), sampleRate));

    // Wavetable mip: pick the harmonic cap that stays alias-free at this pitch.
    wtMip = WavetableBank::mipForIncrement (phaseInc);
}

void ExcitationCore::trigger() noexcept
{
    // Re-arm the transient envelope. Decay length depends on the source: a
    // dirac-like Impulse is gone almost instantly, a Pizzicato is a short
    // pluck, a Spark a quick zap, everything else a slower mallet/strike.
    float decaySeconds = 0.6f;
    switch (type)
    {
        case ExcitationType::Impulse:   decaySeconds = 0.015f; break;
        case ExcitationType::Spark:     decaySeconds = 0.08f;  break;
        case ExcitationType::Pizzicato: decaySeconds = 0.12f;  break;
        default:                        break;
    }

    transientEnv   = 1.0f;
    transientCoeff = std::exp (-1.0f / static_cast<float> (decaySeconds * sampleRate));
    mainPhase      = 0.0;
}

double ExcitationCore::advancePhase (double& phase, double increment) noexcept
{
    phase += increment;
    while (phase >= 1.0) phase -= 1.0;
    return phase;
}

float ExcitationCore::nextDrift() noexcept
{
    // New random target a few times per second; one-pole glide toward it.
    if (--driftCounter <= 0)
    {
        driftTarget  = random.nextFloat() * 2.0f - 1.0f;
        driftCounter = static_cast<int> (sampleRate * 0.10);
    }

    driftState += (driftTarget - driftState) * 0.0008f;
    return driftState;
}

float ExcitationCore::processSample() noexcept
{
    switch (type)
    {
        // IMPACT
        case ExcitationType::Strike:    return renderStrike();
        case ExcitationType::Impulse:   return renderImpulse();
        case ExcitationType::Pizzicato: return renderPizzicato();
        // FRICTION
        case ExcitationType::Bow:       return renderBow();
        case ExcitationType::Scrape:    return renderScrape();
        // AIR
        case ExcitationType::Voice:     return renderVoice();
        case ExcitationType::Breath:    return renderBreath();
        // SYNTH
        case ExcitationType::Spark:     return renderSpark();
        case ExcitationType::Sine:      return renderSine();
        // NOISE
        case ExcitationType::Noise:     return renderNoise();
        // TAPE
        case ExcitationType::Tape:      return renderTape();
        case ExcitationType::TapeLoop:  return renderTapeLoop();
        // SYNTH (wavetable family) — all share the band-limited bank reader,
        // differing only by the shape selected in setType().
        case ExcitationType::Wavetable:
        case ExcitationType::WtPwm:
        case ExcitationType::WtFormant:
        case ExcitationType::WtMetallic:
        case ExcitationType::WtEPiano:
        case ExcitationType::WtDigital: return renderWavetable();
        case ExcitationType::NumTypes:
        default:                        return 0.0f;
    }
}

float ExcitationCore::renderBow() noexcept
{
    // Continuously driven tone: small, slow pitch instability + a touch of
    // odd harmonic to suggest the friction of a bow.
    const float drift = nextDrift() * 0.004f;
    advancePhase (mainPhase, phaseInc * (1.0 + drift));

    const float fundamental = fastSine (mainPhase);
    const float third       = fastSine (std::fmod (mainPhase * 3.0, 1.0)) * 0.18f;
    return (fundamental + third) * 0.7f;
}

float ExcitationCore::renderScrape() noexcept
{
    // A rougher cousin of Bow: deeper, faster pitch drift, stronger odd
    // harmonics and a band of rosin grit (re-using the note-tuned noise
    // band-pass). Less pitched, much grittier than the smooth Bow.
    const float drift = nextDrift() * 0.012f;
    advancePhase (mainPhase, phaseInc * (1.0 + drift));

    const float fundamental = fastSine (mainPhase);
    const float third       = fastSine (std::fmod (mainPhase * 3.0, 1.0)) * 0.28f;
    const float fifth       = fastSine (std::fmod (mainPhase * 5.0, 1.0)) * 0.16f;

    const float white = random.nextFloat() * 2.0f - 1.0f;
    const float grit  = noiseBand.processSample (0, white);

    return (fundamental * 0.5f + third + fifth) * 0.55f + grit * 0.35f;
}

float ExcitationCore::renderStrike() noexcept
{
    // Impulse exciting a tonal decay: a decaying sine, plus a brief noise
    // click on the very front of the transient.
    transientEnv *= transientCoeff;
    advancePhase (mainPhase, phaseInc);

    const float tone  = fastSine (mainPhase) * transientEnv;
    const float click = (random.nextFloat() * 2.0f - 1.0f)
                        * transientEnv * transientEnv * transientEnv * 0.4f;
    return tone + click;
}

float ExcitationCore::renderImpulse() noexcept
{
    // Near-dirac: a very short broadband burst with no tone of its own. This
    // is the cleanest way to "pluck" the primary resonator — the waveguide
    // turns the click into pitch, exactly as a real string does.
    transientEnv *= transientCoeff;
    return (random.nextFloat() * 2.0f - 1.0f) * transientEnv * 0.9f;
}

float ExcitationCore::renderPizzicato() noexcept
{
    // A short plucked body: a noisy front edge over a quickly decaying tone at
    // the note. Sharper and shorter than Strike (which rings as its own mallet
    // tone); fuller than the toneless Impulse.
    transientEnv *= transientCoeff;
    advancePhase (mainPhase, phaseInc);

    const float body  = fastSine (mainPhase) * transientEnv;
    const float front = (random.nextFloat() * 2.0f - 1.0f)
                        * transientEnv * transientEnv * 0.5f;
    return body * 0.8f + front;
}

float ExcitationCore::renderTape() noexcept
{
    // Sawtooth with wow (slow) + flutter (fast) pitch modulation, then light
    // tanh saturation for the worn-tape character.
    const double wow     = fastSine (advancePhase (wowPhase,     0.6 / sampleRate)) * 0.010;
    const double flutter = fastSine (advancePhase (flutterPhase, 7.0 / sampleRate)) * 0.003;

    const double inc = phaseInc * (1.0 + wow + flutter);
    advancePhase (mainPhase, inc);

    const float saw = (static_cast<float> (mainPhase) * 2.0f - 1.0f)
                      - polyBlep (mainPhase, inc);

    // Record-head gap loss: roll off the saw's top octaves (2x one-pole at
    // ~7 kHz) before the tape saturation, so the tanh has nothing up there to
    // fold back across Nyquist. This is also simply what tape sounds like.
    tapeTone1 += (saw - tapeTone1) * tapeToneCoeff;
    tapeTone2 += (tapeTone1 - tapeTone2) * tapeToneCoeff;

    return std::tanh (tapeTone2 * 1.8f) * 0.6f;
}

float ExcitationCore::renderTapeLoop() noexcept
{
    // Tape, but a more degraded loop: heavier flutter and a slow worn-splice
    // dropout that dips the level once per loop revolution.
    const double wow     = fastSine (advancePhase (wowPhase,     0.5 / sampleRate)) * 0.015;
    const double flutter = fastSine (advancePhase (flutterPhase, 11.0 / sampleRate)) * 0.008;

    const double inc = phaseInc * (1.0 + wow + flutter);
    advancePhase (mainPhase, inc);

    const float saw = (static_cast<float> (mainPhase) * 2.0f - 1.0f)
                      - polyBlep (mainPhase, inc);

    tapeTone1 += (saw - tapeTone1) * tapeToneCoeff;
    tapeTone2 += (tapeTone1 - tapeTone2) * tapeToneCoeff;

    // Worn-splice dropout: a ~0.7 Hz amplitude dip to 55 %, like a tired loop
    // passing its splice each revolution.
    const float seam    = fastSine (advancePhase (loopPhase, 0.7 / sampleRate));
    const float dropout = 0.55f + 0.45f * (0.5f + 0.5f * seam);

    return std::tanh (tapeTone2 * 2.2f) * 0.6f * dropout;
}

float ExcitationCore::renderVoice() noexcept
{
    // A buzzy pulse fed through two band-passes acting as formants.
    advancePhase (mainPhase, phaseInc);

    constexpr double duty = 0.45;
    double fallPhase = mainPhase - duty;            // phase relative to the falling edge
    if (fallPhase < 0.0)
        fallPhase += 1.0;

    const float source = ((mainPhase < duty) ? 1.0f : -1.0f)   // asymmetric pulse
                         + polyBlep (mainPhase, phaseInc)
                         - polyBlep (fallPhase, phaseInc);

    const float f1 = formantLow.processSample  (0, source);
    const float f2 = formantHigh.processSample (0, source);
    // The resonant formants peak well above unity; scale so Voice sits at the
    // same loudness as the other sources.
    return (f1 * 0.8f + f2 * 0.5f) * 0.6f;
}

float ExcitationCore::renderBreath() noexcept
{
    // Mostly air: band-passed breath noise (the upper formant used as a soft
    // air resonance) with only a faint pitched whistle on top. The flute/voice
    // "blow" rather than the buzzed "voice".
    advancePhase (mainPhase, phaseInc);

    const float white   = random.nextFloat() * 2.0f - 1.0f;
    const float air     = formantHigh.processSample (0, white * 0.8f);
    const float whistle = fastSine (mainPhase) * 0.15f;

    return air * 0.7f + whistle;
}

float ExcitationCore::renderNoise() noexcept
{
    const float white = random.nextFloat() * 2.0f - 1.0f;
    return noiseBand.processSample (0, white) * 0.9f;
}

float ExcitationCore::renderSpark() noexcept
{
    // Short noisy burst with a bright resonant ping on top.
    transientEnv *= transientCoeff;

    const float white = (random.nextFloat() * 2.0f - 1.0f) * transientEnv;
    const float ping  = sparkBand.processSample (0, white);

    advancePhase (mainPhase, phaseInc * 4.0);
    const float tone = fastSine (mainPhase) * transientEnv * transientEnv * 0.3f;
    return ping + tone;
}

float ExcitationCore::renderSine() noexcept
{
    // The clean reference: a pure sine at the note. Pairs well with the
    // primary resonator for sub basses and glassy sine-fed strings.
    advancePhase (mainPhase, phaseInc);
    return fastSine (mainPhase) * 0.8f;
}

float ExcitationCore::renderWavetable() noexcept
{
    // A real band-limited wavetable: the WAVE knob (wtMorphBias) scans the
    // morph frames, the pre-computed mip keeps it alias-free at any pitch.
    advancePhase (wtPhase, phaseInc);

    return WavetableBank::get().read (static_cast<WavetableBank::Shape> (wtShape),
                                      wtMorphBias, wtPhase, wtMip) * 0.8f;
}
}
