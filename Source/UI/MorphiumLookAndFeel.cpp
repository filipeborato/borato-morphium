#include "MorphiumLookAndFeel.h"

namespace morphium
{
const juce::Colour MorphiumLookAndFeel::metalLight { 0xff35165c }; // Cyber Indigo Light
const juce::Colour MorphiumLookAndFeel::metalDark  { 0xff080212 }; // Midnight Purple Dark
const juce::Colour MorphiumLookAndFeel::gold       { 0xffff007f }; // Neon Laser Pink
const juce::Colour MorphiumLookAndFeel::pointer    { 0xff00f0ff }; // Electric Cyber Cyan
const juce::Colour MorphiumLookAndFeel::slotDark   { 0xff0d011c }; // Recessed Slot Dark
const juce::Colour MorphiumLookAndFeel::ledCyan    { 0xff00f0ff }; // Glowing Cyber Cyan LED

MorphiumLookAndFeel::MorphiumLookAndFeel()
{
    setColour (juce::ComboBox::backgroundColourId, juce::Colour { 0xff0c0217 });
    setColour (juce::ComboBox::textColourId,       juce::Colour { 0xff00f0ff });
    setColour (juce::ComboBox::outlineColourId,    juce::Colour { 0xffff007f });
    setColour (juce::ComboBox::arrowColourId,      pointer);
    setColour (juce::PopupMenu::backgroundColourId, juce::Colour { 0xff0c0217 });
    setColour (juce::PopupMenu::textColourId,       juce::Colour { 0xff00f0ff });
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

    // Outer bezel (neon glow ring).
    g.setColour (gold.withAlpha (0.15f));
    g.fillEllipse (bounds);

    // Knob body with a subtle vertical cyberpunk gradient.
    const auto knob = bounds.reduced (radius * 0.18f);
    g.setGradientFill ({ metalLight, knob.getCentreX(), knob.getY(),
                         metalDark,  knob.getCentreX(), knob.getBottom(), false });
    g.fillEllipse (knob);
    
    // Glowing pink rim outline on the knob body.
    g.setColour (gold.withAlpha (0.6f));
    g.drawEllipse (knob, 1.5f);

    // Value arc (Laser Pink).
    juce::Path arc;
    arc.addCentredArc (centre.x, centre.y, radius * 0.92f, radius * 0.92f,
                       0.0f, startAngle, angle, true);
    g.setColour (gold);
    g.strokePath (arc, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved,
                                             juce::PathStrokeType::rounded));

    // Pointer (Cyber Cyan).
    const float pointerLength = radius * 0.62f;
    juce::Point<float> tip { centre.x + pointerLength * std::sin (angle),
                             centre.y - pointerLength * std::cos (angle) };
    
    // Draw a subtle line glow behind the pointer.
    g.setColour (pointer.withAlpha (0.4f));
    g.drawLine ({ centre, tip }, juce::jmax (4.0f, radius * 0.16f));
    g.setColour (pointer);
    g.drawLine ({ centre, tip }, juce::jmax (2.0f, radius * 0.09f));
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

        // Cap fill (dark metal gradient)
        g.setGradientFill ({ metalLight, cap.getCentreX(), cap.getY(),
                         metalDark,  cap.getCentreX(), cap.getBottom(), false });
        g.fillRoundedRectangle (cap, 4.0f);
        
        // Neon Laser Pink center stripe
        g.setColour (gold);
        g.fillRect (juce::Rectangle<float> (cap.getX() + 3.0f, cap.getCentreY() - 1.5f,
                                            cap.getWidth() - 6.0f, 3.0f));
        
        // Cyber Cyan glowing outer border
        g.setColour (pointer.withAlpha (0.85f));
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
