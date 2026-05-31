#pragma once

#include <juce_audio_utils/juce_audio_utils.h>

namespace morphium
{
    /**
        Sound description for the synthesiser. The MVP has a single timbre that
        responds to every note and channel; the matter/excitation character is
        driven entirely by parameters, not by note ranges.
    */
    class MorphiumSound : public juce::SynthesiserSound
    {
    public:
        bool appliesToNote (int /*midiNoteNumber*/) override    { return true; }
        bool appliesToChannel (int /*midiChannel*/) override     { return true; }
    };
}
