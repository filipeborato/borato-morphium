#include "PluginEditor.h"

#include "BinaryData.h"
#include "Parameters/ParameterIDs.h"

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
    constexpr float kMaxScale     = 1.30f;

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

    // Stylised waveform per excitation source, x in [0, 1), output in [-1, 1].
    float waveSample (int type, float x, float phase) noexcept
    {
        constexpr float tau = juce::MathConstants<float>::twoPi;

        switch (type)
        {
            case 0: // Bow: smooth tone with a faint upper harmonic
                return 0.78f * std::sin (tau * (x * 2.0f + phase))
                     + 0.16f * std::sin (tau * (x * 6.0f + phase));

            case 1: // Strike: a struck, decaying tone
            {
                const float env = std::exp (-x * 3.4f);
                return env * std::sin (tau * (x * 5.0f + phase * 0.25f));
            }

            case 2: // Tape: wobbling sawtooth
            {
                const float wobble = 0.05f * std::sin (tau * (x * 0.5f + phase));
                const float t = std::fmod (x * 2.5f + phase + wobble + 4.0f, 1.0f);
                return (t * 2.0f - 1.0f) * 0.78f;
            }

            case 3: // Voice: two formant-like partials
                return 0.5f * std::sin (tau * (x * 3.0f + phase))
                     + 0.4f * std::sin (tau * (x * 7.0f + phase * 1.3f));

            case 4: // Noise: filtered jitter
                return 0.7f * scrollNoise (x, phase, 911.0f);

            case 5: // Spark: bright transient + noisy decay
            {
                const float env = std::exp (-x * 7.5f);
                const float spike = (x < 0.04f) ? 1.0f : 0.0f;
                return juce::jlimit (-1.0f, 1.0f,
                                     env * (spike + 0.55f * scrollNoise (x, phase, 1300.0f)));
            }

            default:
                return 0.0f;
        }
    }
}

void WaveDisplay::paint (juce::Graphics& g)
{
    const auto area = getLocalBounds().toFloat().reduced (getWidth() * 0.03f, getHeight() * 0.12f);
    const int type = excitationParam != nullptr
                       ? juce::jlimit (0, 5, juce::roundToInt (excitationParam->load()))
                       : 0;

    juce::Path path;
    constexpr int points = 110;
    for (int i = 0; i <= points; ++i)
    {
        const float x  = (float) i / (float) points;
        const float v  = waveSample (type, x, phase);
        const float px = area.getX() + x * area.getWidth();
        const float py = area.getCentreY() - v * area.getHeight() * 0.42f;

        if (i == 0) path.startNewSubPath (px, py);
        else        path.lineTo (px, py);
    }

    const juce::Colour trace { 0xff78ffca };
    const float lineW = juce::jmax (1.5f, getHeight() * 0.03f);
    g.setColour (trace.withAlpha (0.22f));
    g.strokePath (path, juce::PathStrokeType (lineW * 2.4f, juce::PathStrokeType::curved));
    g.setColour (trace);
    g.strokePath (path, juce::PathStrokeType (lineW, juce::PathStrokeType::curved));
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
    const auto metalLt = MorphiumLookAndFeel::metalLight;
    const auto metalDk = MorphiumLookAndFeel::metalDark;
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

    // Knob body with shadow + metal gradient.
    const float bodyR = radius * 0.64f;
    const auto body = juce::Rectangle<float> (bodyR * 2.0f, bodyR * 2.0f).withCentre (centre);
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.fillEllipse (body.translated (0.0f, radius * 0.04f).expanded (radius * 0.03f));
    g.setGradientFill ({ metalLt, body.getCentreX(), body.getY(),
                         metalDk, body.getCentreX(), body.getBottom(), false });
    g.fillEllipse (body);
    g.setColour (MorphiumLookAndFeel::slotDark);
    g.drawEllipse (body, 2.5f);
    g.setColour (juce::Colours::white.withAlpha (0.06f));
    g.drawEllipse (body.reduced (bodyR * 0.16f), 1.5f);

    // Pointer.
    const juce::Point<float> tip  { centre.x + bodyR * 0.80f * std::sin (angle),
                                    centre.y - bodyR * 0.80f * std::cos (angle) };
    const juce::Point<float> base { centre.x + bodyR * 0.16f * std::sin (angle),
                                    centre.y - bodyR * 0.16f * std::cos (angle) };
    g.setColour (MorphiumLookAndFeel::pointer);
    g.drawLine ({ base, tip }, juce::jmax (2.5f, radius * 0.085f));

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
    if (onStep != nullptr)
        onStep (e.position.x < getWidth() * 0.5f ? -1 : +1);
}

void PresetDisplay::paint (juce::Graphics& g)
{
    const float h = (float) getHeight();
    const float w = (float) getWidth();
    auto bounds = getLocalBounds().toFloat().reduced (w * 0.05f, h * 0.14f);
    const auto red = MorphiumLookAndFeel::ledRed;

    auto drawLed = [&] (const juce::String& text, juce::Rectangle<float> area,
                        float height, juce::Justification just)
    {
        const juce::Font font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(),
                                                  height, juce::Font::bold));
        g.setFont (font);
        g.setColour (red.withAlpha (0.25f));            // faint bloom
        g.drawText (text, area.translated (0.0f, 0.5f), just, false);
        g.setColour (red);
        g.drawText (text, area, just, false);
    };

    drawLed ("PRESET " + juce::String (presetIndex + 1).paddedLeft ('0', 2),
             bounds.removeFromTop (h * 0.30f), h * 0.22f, juce::Justification::topLeft);
    drawLed (presetName, bounds, h * 0.40f, juce::Justification::centredLeft);

    // Clickable chevrons, brighter when the pointer is over the display.
    const bool over = isMouseOver();
    auto full = getLocalBounds().toFloat();
    g.setColour (red.withAlpha (over ? 0.85f : 0.35f));
    g.setFont (juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(),
                                              h * 0.36f, juce::Font::bold)));
    g.drawText ("<", full.removeFromLeft (w * 0.05f),  juce::Justification::centred, false);
    g.drawText (">", full.removeFromRight (w * 0.05f), juce::Justification::centred, false);
}

void SourceButton::paintButton (juce::Graphics& g, bool highlighted, bool /*down*/)
{
    const auto bounds = getLocalBounds().toFloat();

    if (selected)
    {
        g.setColour (MorphiumLookAndFeel::gold.withAlpha (0.95f));
        g.drawRoundedRectangle (bounds.reduced (1.5f), 6.0f, 2.0f);
    }
    else if (highlighted)
    {
        g.setColour (juce::Colours::white.withAlpha (0.07f));
        g.fillRoundedRectangle (bounds, 6.0f);
    }

    // LED, positioned like the original SVG (~82% across, top quarter).
    const float ledSize = bounds.getHeight() * 0.19f;
    const auto led = juce::Rectangle<float> (ledSize, ledSize)
                         .withCentre ({ bounds.getX() + bounds.getWidth() * 0.82f,
                                        bounds.getY() + bounds.getHeight() * 0.25f });
    if (selected)
    {
        g.setColour (MorphiumLookAndFeel::ledRed.withAlpha (0.35f));
        g.fillEllipse (led.expanded (ledSize * 0.45f));
        g.setColour (MorphiumLookAndFeel::ledRed);
    }
    else
    {
        g.setColour (juce::Colour { 0xff3a2222 });
    }
    g.fillEllipse (led);
}

MorphiumAudioProcessorEditor::MorphiumAudioProcessorEditor (MorphiumAudioProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p)
{
    setLookAndFeel (&lookAndFeel);

    panel = juce::Drawable::createFromImageData (BinaryData::morphium_panel_svg,
                                                 BinaryData::morphium_panel_svgSize);

    // --- Preset display -----------------------------------------------------
    presetDisplay.onStep = [this] (int direction) { stepPreset (direction); };
    addAndMakeVisible (presetDisplay);
    refreshPresetDisplay();

    auto& apvts = processorRef.getValueTreeState();

    // --- Waveform scope -----------------------------------------------------
    waveDisplay.setSource (apvts.getRawParameterValue (params::excitationType));
    addAndMakeVisible (waveDisplay);
    startTimerHz (30);  // animate the scope + keep the preset display in sync

    // --- Excitation source (the six SOURCE buttons) -------------------------
    for (int i = 0; i < numSources; ++i)
    {
        auto& button = sourceButtons[(size_t) i];
        button.onClick = [this, i]
        {
            if (excitationAttachment != nullptr)
                excitationAttachment->setValueAsCompleteGesture (static_cast<float> (i));
        };
        addAndMakeVisible (button);
    }

    if (auto* excitationParam = apvts.getParameter (params::excitationType))
    {
        excitationAttachment = std::make_unique<juce::ParameterAttachment> (
            *excitationParam,
            [this] (float value)
            {
                const int index = juce::jlimit (0, numSources - 1, juce::roundToInt (value));
                for (int i = 0; i < numSources; ++i)
                    sourceButtons[(size_t) i].setSelectedState (i == index);
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

    // --- Envelope + output knobs -------------------------------------------
    for (auto* s : { &attackSlider, &decaySlider, &sustainSlider, &releaseSlider, &outputSlider })
        configureRotary (*s);

    attackAtt  = std::make_unique<SliderAttachment> (apvts, params::attack,     attackSlider);
    decayAtt   = std::make_unique<SliderAttachment> (apvts, params::decay,      decaySlider);
    sustainAtt = std::make_unique<SliderAttachment> (apvts, params::sustain,    sustainSlider);
    releaseAtt = std::make_unique<SliderAttachment> (apvts, params::release,    releaseSlider);
    outputAtt  = std::make_unique<SliderAttachment> (apvts, params::outputGain, outputSlider);

    // --- Borato Macro -------------------------------------------------------
    // Not bound to a single parameter: it drives Density + Wear together,
    // a first taste of the morph engine. Future work moves this to a real
    // macro/modulation matrix.
    configureRotary (macroSlider);
    macroSlider.setRange (0.0, 1.0, 0.0);
    macroSlider.setValue (0.5, juce::dontSendNotification);
    macroSlider.onValueChange = [this]
    {
        // Morph the whole "matter" character through three curated states:
        //   A (air / glass) -> B (body / wood) -> C (mass / metal),
        // eased per segment so the centre lands exactly on the B sweet spot.
        const auto state = morphMatter (static_cast<float> (macroSlider.getValue()));

        auto set = [this] (const char* id, float value)
        {
            if (auto* p = processorRef.getValueTreeState().getParameter (id))
                p->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, value));
        };

        set (params::density,  state.density);
        set (params::mass,     state.mass);
        set (params::friction, state.friction);
        set (params::wear,     state.wear);
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
    const int index = processorRef.getCurrentProgram();
    presetDisplay.setContent (index, processorRef.getProgramName (index));
}

juce::Rectangle<int> MorphiumAudioProcessorEditor::svg (float x, float y, float w, float h) const
{
    // Scale derives from the current width, so every control tracks the window
    // size. The aspect ratio is locked by the constrainer, so width is enough.
    const float scale = (float) getWidth() / kSvgWidth;
    return juce::Rectangle<int> (juce::roundToInt (x * scale), juce::roundToInt (y * scale),
                                 juce::roundToInt (w * scale), juce::roundToInt (h * scale));
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

void MorphiumAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour { 0xff191918 });

    if (panel != nullptr)
        panel->drawWithin (g, getLocalBounds().toFloat(),
                           juce::RectanglePlacement::stretchToFit, 1.0f);
}

void MorphiumAudioProcessorEditor::resized()
{
    // Coordinates below are taken directly from the SVG panel zones.

    // Preset display, over the left "glass" screen (SVG glass at 147,227).
    presetDisplay.setBounds (svg (147, 227, 396, 81));

    // Scope, over the right "glass" screen (SVG inner glass at 608,228).
    waveDisplay.setBounds (svg (608, 228, 224, 79));

    // SOURCE block (top right): two rows of three buttons.
    // Base (885,215); each button 78x36, 88 apart horizontally, rows 48 apart.
    for (int i = 0; i < numSources; ++i)
    {
        const int col = i % 3;
        const int row = i / 3;
        sourceButtons[(size_t) i].setBounds (svg (885.0f + col * 88.0f,
                                                  215.0f + row * 48.0f, 78.0f, 36.0f));
    }

    // MATTER faders: base (465,450), 44 wide, 64 apart, 150 tall.
    densitySlider.setBounds  (svg (465, 450, 44, 150));
    massSlider.setBounds     (svg (529, 450, 44, 150));
    frictionSlider.setBounds (svg (593, 450, 44, 150));
    wearSlider.setBounds     (svg (657, 450, 44, 150));

    // CORE knobs -> A / D / S (centres 180/280/380, y 490, r ~43).
    attackSlider.setBounds  (svg (137, 447, 86, 86));
    decaySlider.setBounds   (svg (237, 447, 86, 86));
    sustainSlider.setBounds (svg (337, 447, 86, 86));

    // MOTION/SPACE knobs -> Release / Output (centres 1005/1100, y 487).
    releaseSlider.setBounds (svg (967, 449, 76, 76));
    outputSlider.setBounds  (svg (1062, 449, 76, 76));

    // BORATO MACRO: the hero control. The bounds include an outer margin for
    // the A/B/C labels; the visible knob fills ~72% of it. Centred at (830,538).
    macroSlider.setBounds (svg (722, 430, 216, 216));
}
}
