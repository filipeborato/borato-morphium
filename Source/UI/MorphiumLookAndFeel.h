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
        static const juce::Colour metalLight, metalDark, gold, pointer, slotDark, ledCyan;

        // Custom ColourId for per-slider accent color (value arc + rim).
        // Set via slider.setColour (accentColourId, ...) to override the
        // default gold for a section.
        static constexpr int accentColourId = 0x10000;

        // ---- Shared lighting model -----------------------------------------
        // A single light direction (top, slightly left) keeps every control's
        // shading consistent so the panel reads as one physical surface.
        // These helpers stack to build volume: contact shadow -> convex body
        // -> bevel rim -> specular highlight.

        // Soft drop/contact shadow that seats a round control on the panel.
        static void drawContactShadow (juce::Graphics&, juce::Rectangle<float> body,
                                       float strength = 0.5f);

        // Domed metal fill: radial gradient biased toward the light so the disc
        // reads as a sphere/dome rather than a flat coin. `tint` recolours it.
        static void drawConvexDisc (juce::Graphics&, juce::Rectangle<float> body,
                                    juce::Colour hiTint, juce::Colour loTint);

        // Bevelled rim: light along the top edge, dark along the bottom edge.
        static void drawBevelRing (juce::Graphics&, juce::Rectangle<float> body,
                                   float thickness);

        // Off-centre specular bloom (the glossy hot spot).
        static void drawSpecular (juce::Graphics&, juce::Rectangle<float> body,
                                  float alpha = 0.30f);

        // Rounded-rect equivalents for faders / push buttons.
        static void drawRectShadow (juce::Graphics&, juce::Rectangle<float> r,
                                    float corner, float strength = 0.45f);
        static void drawConvexRect (juce::Graphics&, juce::Rectangle<float> r,
                                    float corner, juce::Colour hiTint, juce::Colour loTint);

        // Full realistic knob body in one call: cast shadow, dark socket lip,
        // convex metal dome (lit from the top), bevelled rim, neon identity ring
        // and a glossy specular. Callers add the value arc + pointer on top.
        static void drawKnobBody (juce::Graphics&, juce::Rectangle<float> body,
                                  juce::Colour rimAccent);

        // A clean tapered indicator that reads as sitting on the domed top face.
        static void drawKnobPointer (juce::Graphics&, juce::Rectangle<float> body,
                                     float angle, juce::Colour col);
    };
}
