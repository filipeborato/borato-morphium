#include "ParameterLayout.h"

#include "ParameterIDs.h"
#include "../Core/ExcitationCore.h"

namespace morphium
{
namespace
{
    using APF  = juce::AudioParameterFloat;
    using APC  = juce::AudioParameterChoice;
    using Attr = juce::AudioParameterFloatAttributes;

    juce::ParameterID makeID (const char* id)
    {
        return { id, params::versionHint };
    }

    // A time range (seconds) with a skew that keeps short values usable.
    juce::NormalisableRange<float> timeRange (float maxSeconds)
    {
        juce::NormalisableRange<float> range { 0.001f, maxSeconds, 0.0001f };
        range.setSkewForCentre (0.2f);
        return range;
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // --- Core ---------------------------------------------------------------
    layout.add (std::make_unique<APC> (makeID (params::excitationType),
                                       "Excitation",
                                       getExcitationTypeNames(),
                                       0));

    // --- Matter -------------------------------------------------------------
    const auto unit = juce::NormalisableRange<float> { 0.0f, 1.0f, 0.0001f };

    layout.add (std::make_unique<APF> (makeID (params::density),  "Density",  unit, 0.50f));
    layout.add (std::make_unique<APF> (makeID (params::mass),     "Mass",     unit, 0.50f));
    layout.add (std::make_unique<APF> (makeID (params::friction), "Friction", unit, 0.30f));
    layout.add (std::make_unique<APF> (makeID (params::wear),     "Wear",     unit, 0.20f));

    // --- Amp envelope -------------------------------------------------------
    layout.add (std::make_unique<APF> (makeID (params::attack),  "Attack",
                                       timeRange (5.0f), 0.010f,
                                       Attr().withLabel ("s")));
    layout.add (std::make_unique<APF> (makeID (params::decay),   "Decay",
                                       timeRange (5.0f), 0.200f,
                                       Attr().withLabel ("s")));
    layout.add (std::make_unique<APF> (makeID (params::sustain), "Sustain", unit, 0.80f));
    layout.add (std::make_unique<APF> (makeID (params::release), "Release",
                                       timeRange (8.0f), 0.400f,
                                       Attr().withLabel ("s")));

    // --- Motion (LFO) -------------------------------------------------------
    juce::NormalisableRange<float> rateRange { 0.05f, 20.0f, 0.01f };
    rateRange.setSkewForCentre (1.5f);
    layout.add (std::make_unique<APF> (makeID (params::lfoRate), "LFO Rate", rateRange, 1.0f, Attr().withLabel ("Hz")));
    layout.add (std::make_unique<APF> (makeID (params::lfoDepth), "LFO Depth", unit, 0.20f));

    // --- Space & Color (Reverb, Drive) --------------------------------------
    layout.add (std::make_unique<APF> (makeID (params::reverbSize), "Reverb Size", unit, 0.60f));
    layout.add (std::make_unique<APF> (makeID (params::reverbMix), "Reverb Mix", unit, 0.25f));
    layout.add (std::make_unique<APF> (makeID (params::drive), "Drive", unit, 0.0f));

    // --- Resonator Model ----------------------------------------------------
    layout.add (std::make_unique<APC> (makeID (params::resonatorMode),
                                       "Resonator Model",
                                       juce::StringArray { "OFF", "METAL", "GLASS", "WOOD" },
                                       0));

    // --- Output -------------------------------------------------------------
    layout.add (std::make_unique<APF> (makeID (params::outputGain), "Output",
                                       juce::NormalisableRange<float> { -60.0f, 6.0f, 0.1f },
                                       -6.0f,
                                       Attr().withLabel ("dB")));

    // --- Macro --------------------------------------------------------------
    layout.add (std::make_unique<APF> (makeID (params::macroAmount), "Macro Amount", unit, 0.50f));
    layout.add (std::make_unique<APC> (makeID (params::macroMode), "Macro Mode",
                                       juce::StringArray { "MATTER", "SPACE", "PUNCH", "CHAOS", "FREEZE", "PULSAR", "LOFI", "RESONANCE" }, 0));

    return layout;
}
}
