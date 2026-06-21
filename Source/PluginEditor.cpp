#include "PluginEditor.h"

#include "BinaryData.h"
#include "Parameters/ParameterIDs.h"

#ifndef MORPHIUM_VERSION
#define MORPHIUM_VERSION "0.4.0"
#endif

namespace morphium
{
namespace
{
    // The editor lays everything out in the SVG's native coordinate system and
    // scales by this factor for the actual window size.
    constexpr float kSvgWidth     = 1280.0f;
    constexpr float kSvgHeight    = 760.0f;
    constexpr float kAspectRatio  = kSvgWidth / kSvgHeight;
    constexpr float kInitialScale = 0.85f;
    constexpr float kMinScale     = 0.55f;
    constexpr float kMaxScale     = 3.00f;

    // Three curated "matter states" the Borato Macro morphs through.
    struct MatterState { float density, mass, friction, wear; };
    constexpr MatterState kMatterA { 0.20f, 0.15f, 0.10f, 0.05f }; // A: air / glass
    constexpr MatterState kMatterB { 0.55f, 0.50f, 0.45f, 0.25f }; // B: body / wood
    constexpr MatterState kMatterC { 0.92f, 0.88f, 0.80f, 0.70f }; // C: mass / metal

    MatterState morphMatter (float m) noexcept
    {
        const auto lerp = [] (float a, float b, float t) { return a + (b - a) * t; };
        const auto ease = [] (float t) { return t * t * (3.0f - 2.0f * t); };   // smoothstep
        const auto blend = [&] (const MatterState& x, const MatterState& y, float t)
        {
            return MatterState { lerp (x.density,  y.density,  t),
                                 lerp (x.mass,     y.mass,     t),
                                 lerp (x.friction, y.friction, t),
                                 lerp (x.wear,     y.wear,     t) };
        };

        return (m < 0.5f) ? blend (kMatterA, kMatterB, ease (m / 0.5f))
                          : blend (kMatterB, kMatterC, ease ((m - 0.5f) / 0.5f));
    }

    // A deterministic, scrollable pseudo-noise in [-1, 1].
    float scrollNoise (float x, float phase, float density) noexcept
    {
        const float seed = x * density + std::floor (phase * 24.0f) * 71.0f;
        const float s = std::sin (seed) * 43758.5453f;
        return (s - std::floor (s)) * 2.0f - 1.0f;
    }

    // Stylised waveform per SOURCE family, x in [0, 1), output in [-1, 1].
    // Family order matches the six SOURCE buttons:
    // 0 IMPACT · 1 FRICTION · 2 AIR · 3 SYNTH · 4 NOISE · 5 TAPE.
    float waveSample (int family, float x, float phase) noexcept
    {
        constexpr float tau = juce::MathConstants<float>::twoPi;

        switch (family)
        {
            case 0: // IMPACT: a struck, decaying tone
            {
                const float env = std::exp (-x * 3.4f);
                return env * std::sin (tau * (x * 5.0f + phase * 0.25f));
            }

            case 1: // FRICTION: smooth bowed tone with a faint upper harmonic
                return 0.78f * std::sin (tau * (x * 2.0f + phase))
                     + 0.16f * std::sin (tau * (x * 6.0f + phase));

            case 2: // AIR: two formant-like partials
                return 0.5f * std::sin (tau * (x * 3.0f + phase))
                     + 0.4f * std::sin (tau * (x * 7.0f + phase * 1.3f));

            case 3: // SYNTH: bright transient + noisy decay
            {
                const float env = std::exp (-x * 7.5f);
                const float spike = (x < 0.04f) ? 1.0f : 0.0f;
                return juce::jlimit (-1.0f, 1.0f,
                                     env * (spike + 0.55f * scrollNoise (x, phase, 1300.0f)));
            }

            case 4: // NOISE: filtered jitter
                return 0.7f * scrollNoise (x, phase, 911.0f);

            case 5: // TAPE: wobbling sawtooth
            {
                const float wobble = 0.05f * std::sin (tau * (x * 0.5f + phase));
                const float t = std::fmod (x * 2.5f + phase + wobble + 4.0f, 1.0f);
                return (t * 2.0f - 1.0f) * 0.78f;
            }

            default:
                return 0.0f;
        }
    }
}

int MorphiumAudioProcessorEditor::excitationFamily (int index) noexcept
{
    if      (index >= 11) return 5;   // 11..17 -> TAPE
    else if (index >= 10) return 4;   // 10     -> NOISE
    else if (index >= 7)  return 3;   // 7..9   -> SYNTH
    else if (index >= 5)  return 2;   // 5..6   -> AIR
    else if (index >= 3)  return 1;   // 3..4   -> FRICTION
    else                  return 0;   // 0..2   -> IMPACT
}

void MorphiumAudioProcessorEditor::applyMacro (float amount, int mode)
{
    auto set = [this] (const char* id, float value)
    {
        if (auto* p = processorRef.getValueTreeState().getParameter (id))
            p->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, value));
    };

    switch (mode)
    {
        case 0: // MATTER (Air -> Wood -> Metal)
        {
            auto state = morphMatter (amount);
            set (params::density, state.density);
            set (params::mass, state.mass);
            set (params::friction, state.friction);
            set (params::wear, state.wear);
            break;
        }
        case 1: // SPACE (Dry -> Massive)
            set (params::reverbSize, 0.4f + amount * 0.6f);
            set (params::reverbMix, 0.1f + amount * 0.8f);
            set (params::lfoRate, 0.2f + amount * 0.5f);
            set (params::release, 0.3f + amount * 0.7f);
            break;
        case 2: // PUNCH (Pad -> Pluck)
            set (params::attack, 0.5f * (1.0f - amount));
            set (params::sustain, 1.0f - amount);
            set (params::density, 0.2f + amount * 0.8f);
            break;
        case 3: // CHAOS (Clean -> Broken)
            set (params::wear, amount);
            set (params::friction, amount);
            set (params::lfoDepth, amount);
            set (params::lfoRate, 0.1f + amount * 0.9f);
            break;
        case 4: // FREEZE (Percussive -> Drone)
            set (params::decay, 0.1f + amount * 0.9f);
            set (params::sustain, amount);
            set (params::reverbMix, 0.2f + amount * 0.6f);
            set (params::attack, 0.05f + amount * 0.5f);
            break;
        case 5: // PULSAR (Static -> Rhythmic)
            set (params::lfoRate, 0.5f + amount * 0.5f);
            set (params::lfoDepth, amount);
            set (params::wear, amount * 0.8f);
            break;
        case 6: // LOFI (Full -> Thin/Dusty)
            set (params::density, 1.0f - amount);
            set (params::mass, 1.0f - amount * 0.8f);
            set (params::wear, amount);
            set (params::friction, amount * 0.5f);
            break;
        case 7: // RESONANCE (Damped -> Pure Ring)
            set (params::friction, 0.5f * (1.0f - amount));
            set (params::mass, 0.2f + amount * 0.8f);
            set (params::density, 0.5f + amount * 0.5f);
            set (params::reverbSize, 0.5f * (1.0f - amount));
            break;
    }
}

void MorphiumAudioProcessorEditor::updateMacroHighlight (int mode)
{
    macroAffectedSliders.clear();

    auto addIf = [this] (juce::Slider* s) { macroAffectedSliders.push_back (s); };

    switch (mode)
    {
        case 0: // MATTER
            addIf (&densitySlider); addIf (&massSlider); addIf (&frictionSlider); addIf (&wearSlider);
            break;
        case 1: // SPACE
            addIf (&reverbSizeSlider); addIf (&reverbMixSlider); addIf (&lfoRateSlider); addIf (&releaseSlider);
            break;
        case 2: // PUNCH
            addIf (&attackSlider); addIf (&sustainSlider); addIf (&densitySlider);
            break;
        case 3: // CHAOS
            addIf (&wearSlider); addIf (&frictionSlider); addIf (&lfoDepthSlider); addIf (&lfoRateSlider);
            break;
        case 4: // FREEZE
            addIf (&decaySlider); addIf (&sustainSlider); addIf (&reverbMixSlider); addIf (&attackSlider);
            break;
        case 5: // PULSAR
            addIf (&lfoRateSlider); addIf (&lfoDepthSlider); addIf (&wearSlider);
            break;
        case 6: // LOFI
            addIf (&densitySlider); addIf (&massSlider); addIf (&wearSlider); addIf (&frictionSlider);
            break;
        case 7: // RESONANCE
            addIf (&frictionSlider); addIf (&massSlider); addIf (&densitySlider); addIf (&reverbSizeSlider);
            break;
    }

    macroHighlightFrames = macroHighlightDuration;
    repaint();
}

void WaveDisplay::paint (juce::Graphics& g)
{
    const auto area = getLocalBounds().toFloat().reduced (getWidth() * 0.03f, getHeight() * 0.12f);
    const juce::Colour trace { 0xff00f0ff };
    const float lineW = juce::jmax (1.5f, getHeight() * 0.03f);

    // Try to read real audio from the processor's scope buffer.
    bool drewReal = false;
    if (processor != nullptr)
    {
        // Copy the ring buffer into a local snapshot (lock-free read).
        constexpr int N = MorphiumAudioProcessor::scopeBufferSize;
        std::array<float, N> snapshot;
        const int writePos = processor->scopeWritePos.load (std::memory_order_acquire);
        for (int i = 0; i < N; ++i)
            snapshot[(size_t) i] = processor->scopeBuffer[(size_t) ((writePos + i) % N)];

        // Check if there is enough signal to show.
        float rms = 0.0f;
        for (float s : snapshot)
            rms += s * s;
        rms = std::sqrt (rms / (float) N);

        if (rms > 0.003f)
        {
            drewReal = true;

            // Find peak for auto-scaling.
            float peak = 0.0f;
            for (float s : snapshot)
                peak = juce::jmax (peak, std::abs (s));
            const float scale = (peak > 0.001f) ? (0.42f / peak) : 0.42f;

            juce::Path path;
            for (int i = 0; i < N; ++i)
            {
                const float x = area.getX() + ((float) i / (float) (N - 1)) * area.getWidth();
                const float y = area.getCentreY() - snapshot[(size_t) i] * area.getHeight() * scale;
                if (i == 0) path.startNewSubPath (x, y);
                else        path.lineTo (x, y);
            }

            g.setColour (trace.withAlpha (0.22f));
            g.strokePath (path, juce::PathStrokeType (lineW * 2.4f, juce::PathStrokeType::curved));
            g.setColour (trace);
            g.strokePath (path, juce::PathStrokeType (lineW, juce::PathStrokeType::curved));
        }
    }

    // Fallback: procedural waveform when idle (no audio playing).
    if (! drewReal)
    {
        const int index = excitationParam != nullptr
                            ? juce::roundToInt (excitationParam->load())
                            : 0;
        const int family = MorphiumAudioProcessorEditor::excitationFamily (index);

        juce::Path path;
        constexpr int points = 110;
        for (int i = 0; i <= points; ++i)
        {
            const float x  = (float) i / (float) points;
            const float v  = waveSample (family, x, phase);
            const float px = area.getX() + x * area.getWidth();
            const float py = area.getCentreY() - v * area.getHeight() * 0.42f;

            if (i == 0) path.startNewSubPath (px, py);
            else        path.lineTo (px, py);
        }

        g.setColour (trace.withAlpha (0.22f));
        g.strokePath (path, juce::PathStrokeType (lineW * 2.4f, juce::PathStrokeType::curved));
        g.setColour (trace);
        g.strokePath (path, juce::PathStrokeType (lineW, juce::PathStrokeType::curved));
    }
}

void MacroKnob::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    const auto centre = bounds.getCentre();
    // Reserve an outer margin for the A/B/C labels so they sit clear of the
    // ring and never fall outside the component (which would clip them).
    const float half   = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const float radius = half * 0.72f;

    const float v     = (float) valueToProportionOfLength (getValue());
    const float start = juce::MathConstants<float>::pi * 1.2f;
    const float end   = juce::MathConstants<float>::pi * 2.8f;
    const float angle = start + v * (end - start);

    const auto gold    = MorphiumLookAndFeel::gold;
    const float ringR  = radius * 0.93f;

    // Track ring (full sweep).
    juce::Path track;
    track.addCentredArc (centre.x, centre.y, ringR, ringR, 0.0f, start, end, true);
    g.setColour (juce::Colour { 0xff2a2723 });
    g.strokePath (track, juce::PathStrokeType (radius * 0.11f, juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded));

    // Value arc with a soft bloom.
    juce::Path arc;
    arc.addCentredArc (centre.x, centre.y, ringR, ringR, 0.0f, start, angle, true);
    g.setColour (gold.withAlpha (0.30f));
    g.strokePath (arc, juce::PathStrokeType (radius * 0.22f, juce::PathStrokeType::curved,
                                             juce::PathStrokeType::rounded));
    g.setColour (gold);
    g.strokePath (arc, juce::PathStrokeType (radius * 0.11f, juce::PathStrokeType::curved,
                                             juce::PathStrokeType::rounded));

    // Knob body + pointer use the shared realistic renderer so the hero knob
    // matches every other knob's lighting exactly.
    const float bodyR = radius * 0.64f;
    const auto body = juce::Rectangle<float> (bodyR * 2.0f, bodyR * 2.0f).withCentre (centre);
    MorphiumLookAndFeel::drawKnobBody (g, body, gold);
    MorphiumLookAndFeel::drawKnobPointer (g, body, angle, MorphiumLookAndFeel::pointer);

    // A / B / C markers, set outside the value ring so they never sit on it.
    const float labelR = half * 0.87f;
    g.setFont (juce::Font (juce::FontOptions (juce::jmax (10.0f, radius * 0.17f), juce::Font::bold)));
    auto drawMark = [&] (float frac, const char* text)
    {
        const float a = start + frac * (end - start);
        const juce::Point<float> p { centre.x + labelR * std::sin (a),
                                     centre.y - labelR * std::cos (a) };
        const bool active = std::abs (v - frac) < 0.22f;
        g.setColour (active ? gold.brighter (0.5f) : gold.withAlpha (0.5f));
        g.drawText (text, juce::Rectangle<float> (18.0f, 16.0f).withCentre (p),
                    juce::Justification::centred, false);
    };
    drawMark (0.0f, "A");
    drawMark (0.5f, "B");
    drawMark (1.0f, "C");
}

void PresetDisplay::setContent (int index, const juce::String& name)
{
    if (presetIndex != index || presetName != name)
    {
        presetIndex = index;
        presetName  = name;
        repaint();
    }
}

void PresetDisplay::mouseDown (const juce::MouseEvent& e)
{
    // If there is an onClick handler (like the main preset menu), it owns the center 60%
    if (onClick && e.position.x > getWidth() * 0.2f && e.position.x < getWidth() * 0.8f)
    {
        onClick();
        return;
    }

    // If there is an onStep handler, left half goes back, right half goes forward
    if (onStep)
    {
        if (e.position.x < getWidth() * 0.5f)
            onStep (-1);
        else
            onStep (+1);
    }
}

void PresetDisplay::paint (juce::Graphics& g)
{
    const float h = (float) getHeight();
    // The chevron box must scale with the (height-driven) glyph, otherwise the
    // arrows get clipped to nothing as the editor is resized larger. Pad the
    // text in past the chevrons so they never collide.
    const float arrowW  = juce::jmax (24.0f, h * 0.5f);
    const float sidePad = arrowW + h * 0.10f;
    auto bounds = getLocalBounds().toFloat().reduced (sidePad, h * 0.14f);
    const auto cyan = MorphiumLookAndFeel::ledCyan;

    auto drawLed = [&] (const juce::String& text, juce::Rectangle<float> area,
                        float height, juce::Justification just)
    {
        const juce::Font font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(),
                                                  height, juce::Font::bold));
        g.setFont (font);
        g.setColour (cyan.withAlpha (0.25f));            // faint bloom
        g.drawText (text, area.translated (0.0f, 0.5f), just, false);
        g.setColour (cyan);
        g.drawText (text, area, just, false);
    };

    if (presetIndex >= 0)
    {
        drawLed ("PRESET " + juce::String (presetIndex + 1).paddedLeft ('0', 2),
                 bounds.removeFromTop (h * 0.30f), h * 0.22f, juce::Justification::topLeft);
        drawLed (presetName, bounds, h * 0.40f, juce::Justification::centredLeft);
    }
    else
    {
        drawLed (presetName, bounds, h * 0.40f, juce::Justification::centred);
    }

    // Clickable chevrons, brighter when the pointer is over the display.
    const bool over = isMouseOver();
    auto full = getLocalBounds().toFloat();
    g.setColour (cyan.withAlpha (over ? 0.85f : 0.35f));
    g.setFont (juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(),
                                              h * 0.36f, juce::Font::bold)));
    g.drawText ("<", full.removeFromLeft (arrowW),  juce::Justification::centred, false);
    g.drawText (">", full.removeFromRight (arrowW), juce::Justification::centred, false);
}

void SourceButton::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    const auto bounds = getLocalBounds().toFloat();
    const auto cap = bounds.reduced (1.5f);
    constexpr float corner = 6.0f;

    // Cap relief: domed when up, inset (darker, top inner shadow) when pressed.
    if (down)
    {
        g.setColour (juce::Colours::black.withAlpha (0.28f));
        g.fillRoundedRectangle (cap, corner);
        g.setColour (juce::Colours::black.withAlpha (0.35f));
        g.drawLine (cap.getX() + 4.0f, cap.getY() + 1.5f,
                    cap.getRight() - 4.0f, cap.getY() + 1.5f, 1.5f);   // inner shadow
    }
    else
    {
        MorphiumLookAndFeel::drawConvexRect (g, cap, corner,
                                             juce::Colours::white.withAlpha (highlighted ? 0.14f : 0.08f),
                                             juce::Colours::black.withAlpha (0.18f));
        g.setColour (juce::Colours::white.withAlpha (0.16f));
        g.drawLine (cap.getX() + 4.0f, cap.getY() + 1.5f,
                    cap.getRight() - 4.0f, cap.getY() + 1.5f, 1.0f);   // top highlight
    }

    if (selected)
    {
        g.setColour (MorphiumLookAndFeel::gold.withAlpha (0.95f));
        g.drawRoundedRectangle (cap, corner, 2.0f);
    }

    // Pressing visually sinks the LED + label by one pixel.
    const float sink = down ? 1.0f : 0.0f;

    // LED, positioned top-center for perfect symmetry
    const float ledSize = bounds.getHeight() * 0.16f;
    const auto led = juce::Rectangle<float> (ledSize, ledSize)
                         .withCentre ({ bounds.getCentreX(),
                                        bounds.getY() + 8.0f + sink });
    if (selected)
    {
        g.setColour (MorphiumLookAndFeel::ledCyan.withAlpha (0.35f));
        g.fillEllipse (led.expanded (ledSize * 0.45f));
        g.setColour (MorphiumLookAndFeel::ledCyan);
    }
    else
    {
        g.setColour (juce::Colour { 0xff3a2222 });
    }
    g.fillEllipse (led);
    if (selected)   // tiny specular dot on the lit LED
    {
        g.setColour (juce::Colours::white.withAlpha (0.7f));
        g.fillEllipse (led.reduced (ledSize * 0.32f).translated (-ledSize * 0.1f, -ledSize * 0.1f));
    }

    // Draw text perfectly centered but shifted down slightly to accommodate the LED
    g.setColour (juce::Colour { 0xff00f0ff }); // cyan
    g.setFont (juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(),
                                              bounds.getHeight() * 0.28f, juce::Font::bold)));
    g.drawText (getButtonText(), bounds.withTrimmedTop (8.0f + sink), juce::Justification::centred, false);
}

void ResonatorButton::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    const auto cap = getLocalBounds().toFloat().reduced (1.0f);
    constexpr float corner = 4.0f;

    // Relief: a faint raised cap normally, inset + darkened when pressed.
    if (down)
    {
        g.setColour (juce::Colours::black.withAlpha (0.30f));
        g.fillRoundedRectangle (cap, corner);
        g.setColour (juce::Colours::black.withAlpha (0.35f));
        g.drawLine (cap.getX() + 3.0f, cap.getY() + 1.2f,
                    cap.getRight() - 3.0f, cap.getY() + 1.2f, 1.2f);
    }
    else
    {
        MorphiumLookAndFeel::drawConvexRect (g, cap, corner,
                                             juce::Colours::white.withAlpha (0.07f),
                                             juce::Colours::black.withAlpha (0.16f));
        g.setColour (juce::Colours::white.withAlpha (0.12f));
        g.drawLine (cap.getX() + 3.0f, cap.getY() + 1.2f,
                    cap.getRight() - 3.0f, cap.getY() + 1.2f, 1.0f);
    }

    if (selected)
    {
        // Subtle pink fill background + glowing Neon pink border.
        g.setColour (MorphiumLookAndFeel::gold.withAlpha (0.12f));
        g.fillRoundedRectangle (cap, corner);
        g.setColour (MorphiumLookAndFeel::gold);
        g.drawRoundedRectangle (cap, corner, 1.8f);
    }
    else if (highlighted)
    {
        // Cyber cyan hover feedback border
        g.setColour (MorphiumLookAndFeel::pointer.withAlpha (0.5f));
        g.drawRoundedRectangle (cap, corner, 1.2f);
    }
}

MorphiumAudioProcessorEditor::MorphiumAudioProcessorEditor (MorphiumAudioProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p)
{
    setLookAndFeel (&lookAndFeel);
    setWantsKeyboardFocus (true);   // so Esc reaches keyPressed() for panic

    juce::String svgText (BinaryData::morphium_panel_svg, BinaryData::morphium_panel_svgSize);
    svgText = svgText.replace ("BUILD_VERSION_PLACEHOLDER", "v" + juce::String (MORPHIUM_VERSION));
 
    std::unique_ptr<juce::XmlElement> xml (juce::XmlDocument::parse (svgText));
    if (xml != nullptr)
        panel = juce::Drawable::createFromSVG (*xml);

    // --- Preset display -----------------------------------------------------
    presetDisplay.onStep = [this] (int direction) { stepPreset (direction); };
    presetDisplay.onClick = [this] () { showPresetMenu(); };
    addAndMakeVisible (presetDisplay);
    refreshPresetDisplay();

    auto& apvts = processorRef.getValueTreeState();

    // --- Waveform scope -----------------------------------------------------
    waveDisplay.setSource (apvts.getRawParameterValue (params::excitationType));
    waveDisplay.setProcessor (&processorRef);
    addAndMakeVisible (waveDisplay);

    // WAVE knob over the scope: scans the wavetable morph frames (added after the
    // scope so it draws on top). Only shown for wavetable sources — see the
    // excitation attachment below, which toggles its visibility.
    waveKnob.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    waveKnob.setTooltip ("WAVE - wavetable scan position");
    addChildComponent (waveKnob);   // hidden until a wavetable source is selected
    waveKnobAtt = std::make_unique<SliderAttachment> (apvts, params::wavePosition, waveKnob);

    startTimerHz (30);  // animate the scope + keep the preset display in sync

    // --- Excitation source (the six SOURCE category buttons) ----------------
    const char* sourceNames[] = { "IMPACT", "FRICTION", "AIR", "SYNTH", "NOISE", "TAPE" };
    const int categoryBases[] = {0, 3, 5, 7, 10, 11};
    for (int i = 0; i < numSources; ++i)
    {
        auto& button = sourceButtons[(size_t) i];
        button.setButtonText (sourceNames[i]);
        int baseIdx = categoryBases[i];
        button.onClick = [this, baseIdx]
        {
            if (excitationAttachment != nullptr)
                excitationAttachment->setValueAsCompleteGesture (static_cast<float> (baseIdx));
        };
        addAndMakeVisible (button);
    }
    
    // --- Excitation display (LED under the buttons) -------------------------
    addAndMakeVisible(excitationDisplay);
    excitationDisplay.onStep = [this] (int direction) {
        if (auto* choiceParam = dynamic_cast<juce::AudioParameterChoice*>(processorRef.getValueTreeState().getParameter(params::excitationType))) {
            int currentIdx = choiceParam->getIndex();
            int newIdx = juce::jlimit(0, choiceParam->choices.size() - 1, currentIdx + direction);
            choiceParam->operator=(newIdx);
        }
    };

    if (auto* excitationParam = apvts.getParameter (params::excitationType))
    {
        excitationAttachment = std::make_unique<juce::ParameterAttachment> (
            *excitationParam,
            [this] (float value)
            {
                const int index = juce::roundToInt (value);
                const int category = excitationFamily (index);

                for (int i = 0; i < numSources; ++i)
                    sourceButtons[(size_t) i].setSelectedState (i == category);

                // The WAVE scan knob only does something for wavetable sources,
                // so only show it then (no dead knob on Strike/Bow/Tape/...).
                waveKnob.setVisible (morphium::isWavetableType (index));

                juce::String name = morphium::getExcitationTypeNames()[index];
                excitationDisplay.setContent(-1, "SRC: " + name);
            });
        excitationAttachment->sendInitialUpdate();
    }

    // --- Matter faders ------------------------------------------------------
    for (auto* s : { &densitySlider, &massSlider, &frictionSlider, &wearSlider })
        configureVertical (*s);

    densityAtt  = std::make_unique<SliderAttachment> (apvts, params::density,  densitySlider);
    massAtt     = std::make_unique<SliderAttachment> (apvts, params::mass,     massSlider);
    frictionAtt = std::make_unique<SliderAttachment> (apvts, params::friction, frictionSlider);
    wearAtt     = std::make_unique<SliderAttachment> (apvts, params::wear,     wearSlider);

    densitySlider.setTooltip  ("DENSITY - low-end body + modal inharmonicity");
    massSlider.setTooltip     ("MASS - darkness + spectral tilt");
    frictionSlider.setTooltip ("FRICTION - exciter saturation + body nonlinearity");
    wearSlider.setTooltip     ("WEAR - material ageing, T60 loss, detune");

    // Matter faders: warm amber accent to distinguish from CORE's pink.
    const juce::Colour matterAccent { 0xffff6b00 };
    for (auto* s : { &densitySlider, &massSlider, &frictionSlider, &wearSlider })
        s->setColour (MorphiumLookAndFeel::accentColourId, matterAccent);

    // --- Resonator buttons --------------------------------------------------
    const char* resonatorNames[] = { "OFF", "METAL", "GLASS", "WOOD", "TAPE", "CIRCUIT", "CONCRETE" };
    for (int i = 0; i < numResonatorModes; ++i)
    {
        auto& button = resonatorButtons[(size_t) i];
        button.setButtonText (resonatorNames[i]);
        button.onClick = [this, i]
        {
            if (resonatorAttachment != nullptr)
                resonatorAttachment->setValueAsCompleteGesture (static_cast<float> (i));
        };
        addAndMakeVisible (button);
    }

    if (auto* resonatorParam = apvts.getParameter (params::resonatorMode))
    {
        resonatorAttachment = std::make_unique<juce::ParameterAttachment> (
            *resonatorParam,
            [this] (float value)
            {
                const int index = juce::roundToInt (value);
                for (int i = 0; i < numResonatorModes; ++i)
                    resonatorButtons[(size_t) i].setSelectedState (i == index);
            });
        resonatorAttachment->sendInitialUpdate();
    }

    // --- Envelope + output knobs -------------------------------------------
    for (auto* s : { &attackSlider, &decaySlider, &sustainSlider, &releaseSlider, &outputSlider,
                     &lfoRateSlider, &lfoDepthSlider, &reverbSizeSlider, &reverbMixSlider, &driveSlider })
    {
        configureRotary (*s);
    }

    attackAtt      = std::make_unique<SliderAttachment> (apvts, params::attack,     attackSlider);
    decayAtt       = std::make_unique<SliderAttachment> (apvts, params::decay,      decaySlider);
    sustainAtt     = std::make_unique<SliderAttachment> (apvts, params::sustain,    sustainSlider);
    releaseAtt     = std::make_unique<SliderAttachment> (apvts, params::release,    releaseSlider);
    lfoRateAtt     = std::make_unique<SliderAttachment> (apvts, params::lfoRate,    lfoRateSlider);
    lfoDepthAtt    = std::make_unique<SliderAttachment> (apvts, params::lfoDepth,   lfoDepthSlider);
    reverbSizeAtt  = std::make_unique<SliderAttachment> (apvts, params::reverbSize,  reverbSizeSlider);
    reverbMixAtt   = std::make_unique<SliderAttachment> (apvts, params::reverbMix,   reverbMixSlider);
    driveAtt       = std::make_unique<SliderAttachment> (apvts, params::drive,       driveSlider);
    outputAtt      = std::make_unique<SliderAttachment> (apvts, params::outputGain, outputSlider);

    attackSlider.setTooltip      ("ATTACK - onset time (s)");
    decaySlider.setTooltip       ("DECAY - decay time (s)");
    sustainSlider.setTooltip     ("SUSTAIN - hold level");
    releaseSlider.setTooltip     ("RELEASE - tail time (s)");
    lfoRateSlider.setTooltip     ("LFO RATE - modulation speed (Hz)");
    lfoDepthSlider.setTooltip    ("LFO DEPTH - modulation intensity");
    reverbSizeSlider.setTooltip  ("REVERB SIZE - room scale");
    reverbMixSlider.setTooltip   ("REVERB MIX - wet/dry blend");
    driveSlider.setTooltip       ("DRIVE - soft-clipping saturation");
    outputSlider.setTooltip      ("OUTPUT - master gain (dB)");

    // Motion/Space knobs: cyan accent to tie them to the scope/LED aesthetic.
    for (auto* s : { &lfoRateSlider, &lfoDepthSlider, &reverbSizeSlider,
                     &reverbMixSlider, &driveSlider, &outputSlider })
        s->setColour (MorphiumLookAndFeel::accentColourId, MorphiumLookAndFeel::pointer);

    // --- Output level meter --------------------------------------------------
    addAndMakeVisible (outputMeter);

    // --- Borato Macro -------------------------------------------------------
    addAndMakeVisible (macroDisplay);
    macroDisplay.onStep = [this] (int direction) {
        if (auto* choiceParam = dynamic_cast<juce::AudioParameterChoice*>(processorRef.getValueTreeState().getParameter(params::macroMode))) {
            int currentIdx = choiceParam->getIndex();
            int newIdx = juce::jlimit(0, choiceParam->choices.size() - 1, currentIdx + direction);
            choiceParam->operator=(newIdx);
        }
    };

    if (auto* modeParam = apvts.getParameter(params::macroMode)) {
        macroModeAtt = std::make_unique<juce::ParameterAttachment>(*modeParam, [this](float value) {
            int index = juce::roundToInt(value);
            auto choices = dynamic_cast<juce::AudioParameterChoice*>(processorRef.getValueTreeState().getParameter(params::macroMode))->choices;
            macroDisplay.setContent(-1, "MODE: " + choices[index]);
            updateMacroHighlight (index);
            applyMacro (macroSlider.getValue(), index);
        });
        macroModeAtt->sendInitialUpdate();
    }

    configureRotary (macroSlider);
    macroSlider.setTooltip ("BORATO MACRO - morph engine (A/B/C matter states)");
    macroAtt = std::make_unique<SliderAttachment> (apvts, params::macroAmount, macroSlider);
    macroSlider.onValueChange = [this]
    {
        if (auto* choiceParam = dynamic_cast<juce::AudioParameterChoice*>(processorRef.getValueTreeState().getParameter(params::macroMode))) {
            applyMacro (macroSlider.getValue(), choiceParam->getIndex());
            macroHighlightFrames = macroHighlightDuration;
        }
    };

    // Resizable, with the panel's aspect ratio locked so nothing distorts.
    setResizable (true, true);
    if (auto* bounds = getConstrainer())
    {
        bounds->setFixedAspectRatio (kAspectRatio);
        bounds->setSizeLimits (juce::roundToInt (kSvgWidth * kMinScale),
                               juce::roundToInt (kSvgHeight * kMinScale),
                               juce::roundToInt (kSvgWidth * kMaxScale),
                               juce::roundToInt (kSvgHeight * kMaxScale));
    }

    setSize (juce::roundToInt (kSvgWidth * kInitialScale),
             juce::roundToInt (kSvgHeight * kInitialScale));
}

MorphiumAudioProcessorEditor::~MorphiumAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void MorphiumAudioProcessorEditor::timerCallback()
{
    wavePhase += 0.018f;
    if (wavePhase >= 1.0f)
        wavePhase -= 1.0f;
    waveDisplay.setPhase (wavePhase);

    // Feed the output meter with the latest peak levels from the processor.
    outputMeter.setLevels (processorRef.peakLeft.load (std::memory_order_relaxed),
                           processorRef.peakRight.load (std::memory_order_relaxed));

    // Decay the macro highlight.
    if (macroHighlightFrames > 0)
    {
        --macroHighlightFrames;
        repaint();
    }

    refreshPresetDisplay();
}

void MorphiumAudioProcessorEditor::stepPreset (int direction)
{
    const int total = processorRef.getNumPrograms();
    if (total <= 0)
        return;

    const int next = (processorRef.getCurrentProgram() + direction + total) % total;
    processorRef.setCurrentProgram (next);
    refreshPresetDisplay();
}

void MorphiumAudioProcessorEditor::refreshPresetDisplay()
{
    int index = processorRef.presetManager.currentFactoryIndex;
    juce::String name = processorRef.presetManager.getCurrentPresetName();
    presetDisplay.setContent (index, name);
}

void MorphiumAudioProcessorEditor::showPresetMenu()
{
    juce::PopupMenu menu;
    
    // Factory presets
    juce::PopupMenu factoryMenu;
    const auto& factories = getFactoryPresets();
    for (int i = 0; i < (int)factories.size(); ++i)
    {
        factoryMenu.addItem(i + 1, factories[(size_t)i].name, true, processorRef.presetManager.currentFactoryIndex == i);
    }
    menu.addSubMenu("Factory Presets", factoryMenu);
    
    menu.addSeparator();
    
    // User presets — each in its own submenu with Load / Rename / Delete.
    juce::PopupMenu userMenu;
    auto userPresets = processorRef.presetManager.getUserPresets();
    for (int i = 0; i < userPresets.size(); ++i)
    {
        const juce::String presetName = userPresets[i];
        const bool isActive = processorRef.presetManager.currentUserPresetName == presetName;

        juce::PopupMenu subMenu;
        subMenu.addItem (1000 + i, "Load", true, isActive);
        subMenu.addSeparator();
        subMenu.addItem (3000 + i, "Rename...");
        subMenu.addItem (4000 + i, "Delete", true, true);

        userMenu.addSubMenu (presetName, subMenu, true);
    }
    menu.addSubMenu("User Presets", userMenu);
    
    menu.addSeparator();
    menu.addItem(2000, "Save Preset As...");

    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&presetDisplay),
                       [this, userPresets] (int result)
    {
        if (result == 0) return;
        
        if (result >= 1 && result < 1000)
        {
            processorRef.presetManager.loadFactoryPreset(result - 1);
            refreshPresetDisplay();
        }
        else if (result >= 1000 && result < 2000)
        {
            processorRef.presetManager.loadUserPreset(userPresets[result - 1000]);
            refreshPresetDisplay();
        }
        else if (result == 2000)
        {
            auto* aw = new juce::AlertWindow("Save Preset", "Enter a name for your preset:", juce::MessageBoxIconType::NoIcon);
            aw->addTextEditor("presetName", processorRef.presetManager.getCurrentPresetName(), "");
            aw->addButton("Save", 1, juce::KeyPress(juce::KeyPress::returnKey, 0, 0));
            aw->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey, 0, 0));
            
            aw->enterModalState(true, juce::ModalCallbackFunction::create(
                [this, aw] (int r)
                {
                    if (r == 1)
                    {
                        juce::String name = aw->getTextEditorContents("presetName");
                        if (name.isNotEmpty())
                        {
                            processorRef.presetManager.saveUserPreset(name);
                            refreshPresetDisplay();
                        }
                    }
                    delete aw;
                }));
        }
        else if (result >= 3000 && result < 4000)
        {
            // Rename user preset.
            const juce::String oldName = userPresets[result - 3000];
            auto* aw = new juce::AlertWindow("Rename Preset", "Enter a new name:", juce::MessageBoxIconType::NoIcon);
            aw->addTextEditor("presetName", oldName, "");
            aw->addButton("Rename", 1, juce::KeyPress(juce::KeyPress::returnKey, 0, 0));
            aw->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey, 0, 0));

            aw->enterModalState(true, juce::ModalCallbackFunction::create(
                [this, aw, oldName] (int r)
                {
                    if (r == 1)
                    {
                        juce::String newName = aw->getTextEditorContents("presetName");
                        if (newName.isNotEmpty() && newName != oldName)
                        {
                            processorRef.presetManager.renameUserPreset (oldName, newName);
                            refreshPresetDisplay();
                        }
                    }
                    delete aw;
                }));
        }
        else if (result >= 4000 && result < 5000)
        {
            // Delete user preset (with confirmation).
            const juce::String name = userPresets[result - 4000];
            juce::NativeMessageBox::showYesNoBox (
                juce::MessageBoxIconType::WarningIcon,
                "Delete Preset",
                "Delete preset \"" + name + "\"?",
                nullptr,
                juce::ModalCallbackFunction::create (
                    [this, name] (int r)
                    {
                        if (r == 1)
                        {
                            processorRef.presetManager.deleteUserPreset (name);
                            refreshPresetDisplay();
                        }
                    }));
        }
    });
}

juce::Rectangle<int> MorphiumAudioProcessorEditor::svg (float x, float y, float w, float h) const
{
    // Scale derives from the current width, so every control tracks the window
    // size. The aspect ratio is locked by the constrainer, so width is enough.
    const float scale = (float) getWidth() / kSvgWidth;
    return juce::Rectangle<int> (juce::roundToInt (x * scale), juce::roundToInt (y * scale),
                                 juce::roundToInt (w * scale), juce::roundToInt (h * scale));
}

void WaveKnob::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat().reduced (1.5f);
    const float cx = b.getCentreX();
    const float cy = b.getCentreY();
    const float r  = juce::jmin (b.getWidth(), b.getHeight()) * 0.5f;

    const juce::Colour cyan { 0xff00f0ff };

    // Recessed dark disc so the cyan reads cleanly over the CRT trace, lightly
    // domed with a faint specular so it gains a touch of volume.
    const auto disc = juce::Rectangle<float> (r * 2.0f, r * 2.0f).withCentre ({ cx, cy });
    MorphiumLookAndFeel::drawConvexDisc (g, disc,
                                         juce::Colour { 0xcc1a0a30 }, juce::Colour { 0xdd0d011c });
    g.setColour (cyan.withAlpha (0.45f));
    g.drawEllipse (disc, 1.0f);
    MorphiumLookAndFeel::drawSpecular (g, disc, 0.18f);

    const auto  rp   = getRotaryParameters();
    const float prop = (float) valueToProportionOfLength (getValue());
    const float ang  = rp.startAngleRadians + prop * (rp.endAngleRadians - rp.startAngleRadians);
    const float ar   = r - 2.5f;

    // Value arc (clockwise from the start angle).
    juce::Path arc;
    arc.addCentredArc (cx, cy, ar, ar, 0.0f, rp.startAngleRadians, ang, true);
    g.setColour (cyan);
    g.strokePath (arc, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved,
                                             juce::PathStrokeType::rounded));

    // Pointer (angle measured clockwise from 12 o'clock).
    const float px = cx + ar * std::sin (ang);
    const float py = cy - ar * std::cos (ang);
    g.setColour (cyan);
    g.drawLine (cx, cy, px, py, 1.6f);
    g.fillEllipse (px - 1.8f, py - 1.8f, 3.6f, 3.6f);
}

// --- LevelMeter -------------------------------------------------------------

LevelMeter::LevelMeter()
{
    startTimerHz (30);
}

void LevelMeter::setLevels (float left, float right)
{
    currentLeft  = left;
    currentRight = right;
}

void LevelMeter::timerCallback()
{
    // Decay the displayed levels smoothly.
    constexpr float decay = 0.85f;
    peakHoldLeft  = juce::jmax (currentLeft,  peakHoldLeft  * decay);
    peakHoldRight = juce::jmax (currentRight, peakHoldRight * decay);

    // Peak hold: hold the peak for a few frames, then decay.
    if (currentLeft >= peakHoldLeft * 0.99f)
    {
        peakHoldLeft = currentLeft;
        peakHoldCounterLeft = peakHoldFrames;
    }
    else if (--peakHoldCounterLeft <= 0)
    {
        peakHoldLeft *= decay;
    }

    if (currentRight >= peakHoldRight * 0.99f)
    {
        peakHoldRight = currentRight;
        peakHoldCounterRight = peakHoldFrames;
    }
    else if (--peakHoldCounterRight <= 0)
    {
        peakHoldRight *= decay;
    }

    repaint();
}

void LevelMeter::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    const float gap = 2.0f;
    const float barW = (bounds.getWidth() - gap) * 0.5f;

    auto drawBar = [&] (float x, float level, float peak)
    {
        auto bar = juce::Rectangle<float> (x, bounds.getY(), barW, bounds.getHeight());

        // Background track.
        g.setColour (juce::Colour { 0xff0d011c });
        g.fillRoundedRectangle (bar, 2.0f);

        // Level fill (green -> yellow -> red).
        const float h = bar.getHeight() * juce::jlimit (0.0f, 1.0f, level);
        if (h > 0.0f)
        {
            auto fill = bar.removeFromBottom (h);
            const juce::Colour green  { 0xff00f0ff };
            const juce::Colour yellow { 0xffffff00 };
            const juce::Colour red    { 0xffff007f };

            juce::Colour c = (level > 0.85f) ? red
                           : (level > 0.6f)  ? yellow.withAlpha (0.9f)
                           :                   green;
            g.setColour (c);
            g.fillRoundedRectangle (fill, 2.0f);
        }

        // Peak hold line.
        const float peakY = bar.getBottom() - bar.getHeight() * juce::jlimit (0.0f, 1.0f, peak);
        g.setColour (juce::Colours::white.withAlpha (0.8f));
        g.drawLine (bar.getX(), peakY, bar.getRight(), peakY, 1.5f);
    };

    drawBar (bounds.getX(), currentLeft, peakHoldLeft);
    drawBar (bounds.getX() + barW + gap, currentRight, peakHoldRight);
}

void MorphiumAudioProcessorEditor::configureRotary (juce::Slider& s)
{
    s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    s.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    addAndMakeVisible (s);
}

void MorphiumAudioProcessorEditor::configureVertical (juce::Slider& s)
{
    s.setSliderStyle (juce::Slider::LinearVertical);
    s.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    addAndMakeVisible (s);
}

bool MorphiumAudioProcessorEditor::keyPressed (const juce::KeyPress& key)
{
    // Esc = panic. Bubbles up from any focused knob, so it works even mid-drag.
    if (key == juce::KeyPress::escapeKey)
    {
        processorRef.panic();
        return true;
    }
    return false;
}

void MorphiumAudioProcessorEditor::mouseDoubleClick (const juce::MouseEvent&)
{
    // Double-clicking the chassis (not a control) is a mouse-reachable panic.
    processorRef.panic();
}

void MorphiumAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour { 0xff191918 });

    if (panel != nullptr)
        panel->drawWithin (g, getLocalBounds().toFloat(),
                           juce::RectanglePlacement::stretchToFit, 1.0f);
}

void MorphiumAudioProcessorEditor::paintOverChildren (juce::Graphics& g)
{
    if (macroHighlightFrames <= 0 || macroAffectedSliders.empty())
        return;

    const float alpha = (float) macroHighlightFrames / (float) macroHighlightDuration;
    const auto accent = MorphiumLookAndFeel::gold.withAlpha (alpha * 0.6f);

    for (auto* slider : macroAffectedSliders)
    {
        if (slider == nullptr || ! slider->isVisible())
            continue;

        auto bounds = slider->getBounds().toFloat().reduced (2.0f);
        g.setColour (accent);
        if (slider->getSliderStyle() == juce::Slider::LinearVertical)
            g.drawRoundedRectangle (bounds, 4.0f, 2.5f);
        else
            g.drawEllipse (bounds, 2.5f);
    }
}

void MorphiumAudioProcessorEditor::resized()
{
    // Coordinates below are taken directly from the SVG panel zones.

    // Preset display, over the left "glass" screen (SVG glass at 147,227).
    presetDisplay.setBounds (svg (147, 227, 396, 81));

    // Scope, over the right "glass" screen (SVG inner glass at 608,228).
    waveDisplay.setBounds (svg (608, 228, 224, 79));

    // WAVE knob: a tiny rotary tucked into the scope's top-right corner.
    waveKnob.setBounds (svg (800, 231, 30, 30));

    // SOURCE block (top right): two rows of three buttons.
    // Base (885,215); each button 78x36, 88 apart horizontally, rows 48 apart.
    for (int i = 0; i < numSources; ++i)
    {
        const int col = i % 3;
        const int row = i / 3;
        sourceButtons[(size_t) i].setBounds (svg (885.0f + col * 88.0f,
                                                  215.0f + row * 48.0f, 78.0f, 36.0f));
    }
    
    // LED under the source buttons. SVG: transform="translate(885 311)" size 254x26
    excitationDisplay.setBounds (svg (885, 311, 254, 26));

    // MATTER faders -> Y=468, Height=124 (shifted down 10px, tracks shortened)
    densitySlider.setBounds  (svg (412, 468, 34, 124));
    massSlider.setBounds     (svg (476, 468, 34, 124));
    frictionSlider.setBounds (svg (540, 468, 34, 124));
    wearSlider.setBounds     (svg (604, 468, 34, 124));

    // MACRO
    // BORATO MACRO: the hero control. Centred at (792, 530)
    macroSlider.setBounds  (svg (702, 440, 180, 180));
    macroDisplay.setBounds (svg (732, 635, 120, 22));

    // RESONATOR Model buttons: two rows above faders, matching SVG slots.
    // Row 1 (i<4): OFF, METAL, GLASS, WOOD at y=430, h=14
    // Row 2 (i>=4): TAPE, CIRCUIT, CONCRETE at y=446, h=12
    for (int i = 0; i < numResonatorModes; ++i)
    {
        const int col = i % 4;
        const int row = i / 4;
        const int yy = 430 + row * 16;
        const int hh = 14 - row * 2;
        resonatorButtons[(size_t) i].setBounds (svg (403.0f + (float) col * 64.0f, (float) yy, 52.0f, (float) hh));
    }

    // CORE knobs -> A / D / S / R (2x2 grid)
    attackSlider.setBounds  (svg (143, 454, 72, 72));
    decaySlider.setBounds   (svg (243, 454, 72, 72));
    sustainSlider.setBounds (svg (143, 559, 72, 72));
    releaseSlider.setBounds (svg (243, 559, 72, 72));

    // MOTION/SPACE knobs -> 3 columns x 2 rows grid.
    // Row centers (y=490, y=595) line up with the CORE ADSR rows.
    lfoRateSlider.setBounds    (svg (910, 460, 60, 60));
    reverbSizeSlider.setBounds (svg (990, 460, 60, 60));
    driveSlider.setBounds      (svg (1070, 460, 60, 60));
    
    lfoDepthSlider.setBounds   (svg (910, 565, 60, 60));
    reverbMixSlider.setBounds  (svg (990, 565, 60, 60));
    outputSlider.setBounds     (svg (1070, 565, 60, 60));

    // Output level meter: thin bar right next to the output knob.
    outputMeter.setBounds      (svg (1132, 565, 10, 60));

}
}
