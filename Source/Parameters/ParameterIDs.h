#pragma once

#include <juce_core/juce_core.h>

/**
    Central registry of parameter identifiers.

    Keeping every ID in one place avoids stringly-typed drift between the
    processor, the parameter layout and the editor attachments.
*/
namespace morphium::params
{
    // Core
    inline constexpr auto excitationType = "excitationType";

    // Wavetable scan position (0..1) for the WAVE * shapes in the SYNTH family.
    inline constexpr auto wavePosition = "wavePosition";

    // Matter
    inline constexpr auto density  = "density";
    inline constexpr auto mass     = "mass";
    inline constexpr auto friction = "friction";
    inline constexpr auto wear     = "wear";

    // Amplitude envelope
    inline constexpr auto attack  = "attack";
    inline constexpr auto decay   = "decay";
    inline constexpr auto sustain = "sustain";
    inline constexpr auto release = "release";

    // Motion (LFO)
    inline constexpr auto lfoRate  = "lfoRate";
    inline constexpr auto lfoDepth = "lfoDepth";

    // Space (Reverb & Color)
    inline constexpr auto reverbSize = "reverbSize";
    inline constexpr auto reverbMix  = "reverbMix";
    inline constexpr auto drive      = "drive";

    // Matter Resonator Model
    inline constexpr auto resonatorMode = "resonatorMode";

    // Primary resonator (Karplus-Strong waveguide) dry/wet
    inline constexpr auto waveguideBlend = "waveguideBlend";

    // Output
    inline constexpr auto outputGain = "outputGain";

    // Macro
    inline constexpr auto macroAmount = "macroAmount";
    inline constexpr auto macroMode = "macroMode";

    // All parameters share one version so host automation stays stable.
    inline constexpr int versionHint = 1;
}
