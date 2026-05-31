#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace morphium
{
    /**
        Draws controls in the Borato Morphium panel style (dark metal knobs,
        gold value arcs, recessed fader slots).

        This is intentionally a thin, self-contained layer: when the final SVG
        artwork is ready, the draw methods here can be replaced with
        Drawable-backed rendering without touching the editor or DSP.
    */
    class MorphiumLookAndFeel : public juce::LookAndFeel_V4
    {
    public:
        MorphiumLookAndFeel();

        void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                               float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
                               juce::Slider&) override;

        void drawLinearSlider (juce::Graphics&, int x, int y, int width, int height,
                               float sliderPos, float minSliderPos, float maxSliderPos,
                               juce::Slider::SliderStyle, juce::Slider&) override;

        // Palette shared with the SVG panel.
        static const juce::Colour metalLight, metalDark, gold, pointer, slotDark, ledRed;
    };
}
