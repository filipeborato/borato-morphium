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

    // Space (Reverb)
    inline constexpr auto reverbSize = "reverbSize";
    inline constexpr auto reverbMix  = "reverbMix";

    // Matter Resonator Model
    inline constexpr auto resonatorMode = "resonatorMode";

    // Output
    inline constexpr auto outputGain = "outputGain";

    // Macro
    inline constexpr auto macroAmount = "macroAmount";
    inline constexpr auto macroMode = "macroMode";

    // All parameters share one version so host automation stays stable.
    inline constexpr int versionHint = 1;
}
