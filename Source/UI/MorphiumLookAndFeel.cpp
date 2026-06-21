#include "MorphiumLookAndFeel.h"

namespace morphium
{
const juce::Colour MorphiumLookAndFeel::metalLight { 0xff35165c }; // Cyber Indigo Light
const juce::Colour MorphiumLookAndFeel::metalDark  { 0xff080212 }; // Midnight Purple Dark
const juce::Colour MorphiumLookAndFeel::gold       { 0xffff007f }; // Neon Laser Pink
const juce::Colour MorphiumLookAndFeel::pointer    { 0xff00f0ff }; // Electric Cyber Cyan
const juce::Colour MorphiumLookAndFeel::slotDark   { 0xff0d011c }; // Recessed Slot Dark
const juce::Colour MorphiumLookAndFeel::ledCyan    { 0xff00f0ff }; // Glowing Cyber Cyan LED

// ---- Shared lighting model -------------------------------------------------

void MorphiumLookAndFeel::drawContactShadow (juce::Graphics& g, juce::Rectangle<float> body,
                                             float strength)
{
    const float r = juce::jmin (body.getWidth(), body.getHeight());
    juce::Path p;
    p.addEllipse (body.translated (0.0f, r * 0.06f));
    juce::DropShadow ds (juce::Colours::black.withAlpha (strength),
                         (int) juce::jmax (4.0f, r * 0.16f),
                         { 0, (int) juce::jmax (1.0f, r * 0.05f) });
    ds.drawForPath (g, p);
}

void MorphiumLookAndFeel::drawConvexDisc (juce::Graphics& g, juce::Rectangle<float> body,
                                          juce::Colour hiTint, juce::Colour loTint)
{
    // Highlight sits up-and-left; the gradient falls off toward the lower-right.
    const auto hi = body.getCentre().translated (-body.getWidth()  * 0.20f,
                                                  -body.getHeight() * 0.22f);
    juce::ColourGradient grad (hiTint, hi.x, hi.y,
                               loTint, body.getCentreX(), body.getBottom(), true);
    grad.addColour (0.55, hiTint.interpolatedWith (loTint, 0.55f));
    g.setGradientFill (grad);
    g.fillEllipse (body);
}

void MorphiumLookAndFeel::drawBevelRing (juce::Graphics& g, juce::Rectangle<float> body,
                                         float thickness)
{
    juce::ColourGradient bevel (juce::Colours::white.withAlpha (0.28f),
                                body.getCentreX(), body.getY(),
                                juce::Colours::black.withAlpha (0.40f),
                                body.getCentreX(), body.getBottom(), false);
    g.setGradientFill (bevel);
    g.drawEllipse (body.reduced (thickness * 0.5f), thickness);
}

void MorphiumLookAndFeel::drawSpecular (juce::Graphics& g, juce::Rectangle<float> body,
                                        float alpha)
{
    auto h = body.reduced (body.getWidth() * 0.16f, body.getHeight() * 0.14f);
    h = h.removeFromTop (h.getHeight() * 0.52f).translated (-body.getWidth() * 0.03f, 0.0f);
    juce::ColourGradient bloom (juce::Colours::white.withAlpha (alpha),
                                h.getCentreX(), h.getY() + h.getHeight() * 0.15f,
                                juce::Colours::transparentWhite,
                                h.getCentreX(), h.getBottom(), true);
    g.setGradientFill (bloom);
    g.fillEllipse (h);
}

void MorphiumLookAndFeel::drawRectShadow (juce::Graphics& g, juce::Rectangle<float> r,
                                          float corner, float strength)
{
    juce::Path p;
    p.addRoundedRectangle (r.translated (0.0f, r.getHeight() * 0.10f), corner);
    juce::DropShadow ds (juce::Colours::black.withAlpha (strength),
                         (int) juce::jmax (3.0f, r.getHeight() * 0.22f),
                         { 0, (int) juce::jmax (1.0f, r.getHeight() * 0.08f) });
    ds.drawForPath (g, p);
}

void MorphiumLookAndFeel::drawConvexRect (juce::Graphics& g, juce::Rectangle<float> r,
                                          float corner, juce::Colour hiTint, juce::Colour loTint)
{
    juce::ColourGradient grad (hiTint, r.getCentreX(), r.getY(),
                               loTint, r.getCentreX(), r.getBottom(), false);
    grad.addColour (0.5, hiTint.interpolatedWith (loTint, 0.5f));
    g.setGradientFill (grad);
    g.fillRoundedRectangle (r, corner);
}

void MorphiumLookAndFeel::drawKnobBody (juce::Graphics& g, juce::Rectangle<float> body,
                                        juce::Colour rimAccent)
{
    const float r = juce::jmin (body.getWidth(), body.getHeight()) * 0.5f;
    const auto  c = body.getCentre();

    // A knob is a short metal cylinder: a dark outer side wall, a lit chamfer
    // at the top edge, and a matte, slightly raised top face. It must read flat
    // and metallic — never as a glossy glass ball.

    // 1. Soft cast shadow so the knob seats on the (near-black) panel.
    {
        juce::Path p;
        p.addEllipse (body.translated (0.0f, r * 0.10f));
        juce::DropShadow ds (juce::Colours::black.withAlpha (0.55f),
                             (int) juce::jmax (5.0f, r * 0.35f),
                             { 0, (int) juce::jmax (2.0f, r * 0.10f) });
        ds.drawForPath (g, p);
    }

    // 2. Outer side wall: a dark disc, faintly top-lit so the cylinder edge
    //    catches a little light up top.
    {
        juce::ColourGradient wall (juce::Colour { 0xff262038 }, c.x, body.getY(),
                                   juce::Colour { 0xff080414 }, c.x, body.getBottom(), false);
        g.setGradientFill (wall);
        g.fillEllipse (body);
    }

    // 3. Chamfer ring at the top edge: bright on top, dark at the bottom — the
    //    bevel that sells the raised metal lip.
    {
        juce::ColourGradient bevel (juce::Colours::white.withAlpha (0.35f), c.x, body.getY(),
                                    juce::Colours::black.withAlpha (0.55f), c.x, body.getBottom(), false);
        g.setGradientFill (bevel);
        g.drawEllipse (body.reduced (r * 0.06f), juce::jmax (1.2f, r * 0.07f));
    }

    // 4. Inset top face: the flat, matte top of the knob (gently lit from top).
    const auto face = body.reduced (r * 0.20f);
    {
        juce::ColourGradient top (juce::Colour { 0xff35294f }, c.x, face.getY(),
                                  juce::Colour { 0xff170f2b }, c.x, face.getBottom(), false);
        g.setGradientFill (top);
        g.fillEllipse (face);
    }

    // 5. Fine groove separating the face from the side wall.
    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.drawEllipse (face, juce::jmax (1.0f, r * 0.02f));
    g.setColour (juce::Colours::white.withAlpha (0.08f));
    g.drawEllipse (face.reduced (juce::jmax (1.0f, r * 0.02f)), 1.0f);

    // 6. Thin neon identity ring on the outer edge (subtle).
    g.setColour (rimAccent.withAlpha (0.45f));
    g.drawEllipse (body.reduced (r * 0.015f), juce::jmax (1.0f, r * 0.025f));

    // 7. A very soft, broad sheen on the upper half of the top face only — no
    //    hot-spot, so the surface stays matte metal rather than glass.
    {
        juce::Graphics::ScopedSaveState save (g);
        juce::Path clip; clip.addEllipse (face);
        g.reduceClipRegion (clip);
        auto band = face;
        band = band.removeFromTop (face.getHeight() * 0.55f);
        juce::ColourGradient s (juce::Colours::white.withAlpha (0.10f),
                                band.getCentreX(), band.getY(),
                                juce::Colours::transparentWhite,
                                band.getCentreX(), band.getBottom(), false);
        g.setGradientFill (s);
        g.fillRect (band);
    }
}

void MorphiumLookAndFeel::drawKnobPointer (juce::Graphics& g, juce::Rectangle<float> body,
                                           float angle, juce::Colour col)
{
    const float r = juce::jmin (body.getWidth(), body.getHeight()) * 0.5f;
    const auto  c = body.getCentre();
    // The indicator runs across the matte top face (inset ~0.20r), from near the
    // centre out to just short of the face edge. A clean line — no tip blob.
    const juce::Point<float> tip  { c.x + r * 0.70f * std::sin (angle),
                                    c.y - r * 0.70f * std::cos (angle) };
    const juce::Point<float> base { c.x + r * 0.20f * std::sin (angle),
                                    c.y - r * 0.20f * std::cos (angle) };

    g.setColour (col.withAlpha (0.30f));
    g.drawLine ({ base, tip }, juce::jmax (3.0f, r * 0.13f));   // glow
    g.setColour (col);
    g.drawLine ({ base, tip }, juce::jmax (1.6f, r * 0.06f));   // core
}

MorphiumLookAndFeel::MorphiumLookAndFeel()
{
    // Default accent for all sliders (overridden per-section via setColour).
    setColour (accentColourId, gold);

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
                                            juce::Slider& slider)
{
    const auto bounds = juce::Rectangle<float> (x, y, width, height).reduced (4.0f);
    const auto centre = bounds.getCentre();
    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const float angle  = startAngle + sliderPos * (endAngle - startAngle);

    const auto accent = slider.findColour (accentColourId, true);

    // The knob body fills most of the bounds; the value arc lives in the ring
    // gap outside it, so it reads as an indicator and never as a halo around a
    // dark hole.
    const auto knob = bounds.reduced (radius * 0.20f);
    drawKnobBody (g, knob, accent);

    // Track ring (full sweep) under the value arc.
    const float arcR = radius * 0.93f;
    juce::Path track;
    track.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f, startAngle, endAngle, true);
    g.setColour (juce::Colour { 0xff241830 });
    g.strokePath (track, juce::PathStrokeType (juce::jmax (2.0f, radius * 0.055f),
                                               juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded));

    // Value arc with a soft bloom underneath.
    juce::Path arc;
    arc.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f, startAngle, angle, true);
    g.setColour (accent.withAlpha (0.30f));
    g.strokePath (arc, juce::PathStrokeType (juce::jmax (4.0f, radius * 0.11f),
                                             juce::PathStrokeType::curved,
                                             juce::PathStrokeType::rounded));
    g.setColour (accent);
    g.strokePath (arc, juce::PathStrokeType (juce::jmax (2.0f, radius * 0.055f),
                                             juce::PathStrokeType::curved,
                                             juce::PathStrokeType::rounded));

    drawKnobPointer (g, knob, angle, pointer);
}

void MorphiumLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height,
                                            float sliderPos, float, float,
                                            juce::Slider::SliderStyle style, juce::Slider& slider)
{
    const auto bounds = juce::Rectangle<float> (x, y, width, height);
    const auto accent = slider.findColour (accentColourId, true);

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

        // Drop a soft shadow into the groove, then build the cap with volume:
        // a strong top-lit metal gradient + a glassy highlight band on top.
        drawRectShadow (g, cap, 4.0f, 0.55f);
        drawConvexRect (g, cap, 4.0f, juce::Colour { 0xff8474ad }, juce::Colour { 0xff0c0618 });

        // Top bevel highlight (specular band) and bottom shade line.
        g.setColour (juce::Colours::white.withAlpha (0.32f));
        g.fillRoundedRectangle (cap.withTrimmedBottom (cap.getHeight() * 0.58f)
                                   .reduced (2.0f, 1.5f), 3.0f);
        g.setColour (juce::Colours::black.withAlpha (0.38f));
        g.drawLine (cap.getX() + 3.0f, cap.getBottom() - 2.0f,
                    cap.getRight() - 3.0f, cap.getBottom() - 2.0f, 1.0f);

        // Neon center stripe (with a faint bloom).
        g.setColour (accent.withAlpha (0.35f));
        g.fillRect (juce::Rectangle<float> (cap.getX() + 2.0f, cap.getCentreY() - 2.5f,
                                            cap.getWidth() - 4.0f, 5.0f));
        g.setColour (accent);
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
        g.setColour (accent);
        g.fillRoundedRectangle ({ bounds.getX(), bounds.getCentreY() - 3.0f,
                                  sliderPos - bounds.getX(), 6.0f }, 3.0f);
    }
}
}
