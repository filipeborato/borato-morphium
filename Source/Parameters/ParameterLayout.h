#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace morphium
{
    /** Builds the full AudioProcessorValueTreeState parameter layout. */
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
}
