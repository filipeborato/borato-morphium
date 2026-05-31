#include "MorphiumLookAndFeel.h"

namespace morphium
{
const juce::Colour MorphiumLookAndFeel::metalLight { 0xff585d5f };
const juce::Colour MorphiumLookAndFeel::metalDark  { 0xff101112 };
const juce::Colour MorphiumLookAndFeel::gold       { 0xff8d6d2d };
const juce::Colour MorphiumLookAndFeel::pointer    { 0xfff4efe1 };
const juce::Colour MorphiumLookAndFeel::slotDark   { 0xff070808 };
const juce::Colour MorphiumLookAndFeel::ledRed     { 0xffff3131 };

MorphiumLookAndFeel::MorphiumLookAndFeel()
{
    setColour (juce::ComboBox::backgroundColourId, juce::Colour { 0xff202326 });
    setColour (juce::ComboBox::textColourId,       juce::Colour { 0xffddd4bd });
    setColour (juce::ComboBox::outlineColourId,    juce::Colour { 0xff5f584d });
    setColour (juce::ComboBox::arrowColourId,      gold);
    setColour (juce::PopupMenu::backgroundColourId, juce::Colour { 0xff2a2d30 });
    setColour (juce::PopupMenu::textColourId,       juce::Colour { 0xffddd4bd });
    setColour (juce::PopupMenu::highlightedBackgroundColourId, gold);
}

void MorphiumLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                            float sliderPos, float startAngle, float endAngle,
                                            juce::Slider&)
{
    const auto bounds = juce::Rectangle<float> (x, y, width, height).reduced (4.0f);
    const auto centre = bounds.getCentre();
    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const float angle  = startAngle + sliderPos * (endAngle - startAngle);

    // Outer bezel.
    g.setColour (juce::Colour { 0xff81796c }.withAlpha (0.45f));
    g.fillEllipse (bounds);

    // Knob body with a subtle vertical gradient.
    const auto knob = bounds.reduced (radius * 0.18f);
    g.setGradientFill ({ metalLight, knob.getCentreX(), knob.getY(),
                         metalDark,  knob.getCentreX(), knob.getBottom(), false });
    g.fillEllipse (knob);
    g.setColour (slotDark);
    g.drawEllipse (knob, 2.0f);

    // Value arc.
    juce::Path arc;
    arc.addCentredArc (centre.x, centre.y, radius * 0.92f, radius * 0.92f,
                       0.0f, startAngle, angle, true);
    g.setColour (gold);
    g.strokePath (arc, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved,
                                             juce::PathStrokeType::rounded));

    // Pointer.
    const float pointerLength = radius * 0.62f;
    juce::Point<float> tip { centre.x + pointerLength * std::sin (angle),
                             centre.y - pointerLength * std::cos (angle) };
    g.setColour (pointer);
    g.drawLine ({ centre, tip }, juce::jmax (2.0f, radius * 0.10f));
}

void MorphiumLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height,
                                            float sliderPos, float, float,
                                            juce::Slider::SliderStyle style, juce::Slider&)
{
    const auto bounds = juce::Rectangle<float> (x, y, width, height);

    if (style == juce::Slider::LinearVertical)
    {
        // The recessed groove + centre line are drawn by the SVG panel; here we
        // only render the moving cap so it stays perfectly aligned.
        const float capHeight = juce::jmax (14.0f, bounds.getWidth() * 0.55f);
        const float capWidth  = bounds.getWidth() * 0.86f;
        const auto cap = juce::Rectangle<float> (capWidth, capHeight)
                             .withCentre ({ bounds.getCentreX(),
                                            juce::jlimit (bounds.getY() + capHeight * 0.5f,
                                                          bounds.getBottom() - capHeight * 0.5f,
                                                          sliderPos) });

        g.setGradientFill ({ metalLight, cap.getCentreX(), cap.getY(),
                             metalDark,  cap.getCentreX(), cap.getBottom(), false });
        g.fillRoundedRectangle (cap, 4.0f);
        g.setColour (gold.withAlpha (0.8f));
        g.fillRect (juce::Rectangle<float> (cap.getX() + 3.0f, cap.getCentreY() - 1.0f,
                                            cap.getWidth() - 6.0f, 2.0f));
        g.setColour (juce::Colour { 0xffc8bea8 });
        g.drawRoundedRectangle (cap.reduced (0.75f), 4.0f, 1.5f);
    }
    else
    {
        // Fallback for any other linear style.
        g.setColour (slotDark);
        g.fillRoundedRectangle (bounds.withSizeKeepingCentre (bounds.getWidth(), 6.0f), 3.0f);
        g.setColour (gold);
        g.fillRoundedRectangle ({ bounds.getX(), bounds.getCentreY() - 3.0f,
                                  sliderPos - bounds.getX(), 6.0f }, 3.0f);
    }
}
}
