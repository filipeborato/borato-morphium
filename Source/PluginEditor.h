#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_extra/juce_gui_extra.h>

#include <array>

#include "PluginProcessor.h"
#include "UI/MorphiumLookAndFeel.h"

namespace morphium
{
    /**
        A SOURCE selector button. The panel SVG already draws the button body
        and its label, so this only renders interactive state: a selection
        outline and the red LED, matching the original artwork.
    */
    class SourceButton : public juce::Button
    {
    public:
        SourceButton() : juce::Button ({}) {}

        void setSelectedState (bool shouldBeSelected)
        {
            if (selected != shouldBeSelected) { selected = shouldBeSelected; repaint(); }
        }

        void paintButton (juce::Graphics&, bool highlighted, bool down) override;

    private:
        bool selected = false;
    };

    /**
        A RESONATOR model selector button. Draws a clean glowing outrun pink border
        when selected and cyber cian on hover, fitting the background slots.
    */
    class ResonatorButton : public juce::Button
    {
    public:
        ResonatorButton() : juce::Button ({}) {}

        void setSelectedState (bool shouldBeSelected)
        {
            if (selected != shouldBeSelected) { selected = shouldBeSelected; repaint(); }
        }

        void paintButton (juce::Graphics&, bool highlighted, bool down) override;

    private:
        bool selected = false;
    };

    /**
        The LED preset display. Shows the current factory preset; clicking the
        left half steps to the previous preset, the right half to the next.
    */
    class PresetDisplay : public juce::Component
    {
    public:
        std::function<void (int)> onStep;   // -1 = previous, +1 = next
        std::function<void()> onClick;

        void setContent (int index, const juce::String& name);
        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override;
        void mouseEnter (const juce::MouseEvent&) override { repaint(); }
        void mouseExit  (const juce::MouseEvent&) override { repaint(); }

    private:
        int presetIndex = 0;
        juce::String presetName;
    };

    /**
        A small animated oscilloscope. When audio is playing it shows the real
        output waveform from a lock-free ring buffer in the processor. When
        idle it falls back to a stylised procedural waveform per excitation
        family, scrolling gently so it reads as "alive".
    */
    class WaveDisplay : public juce::Component
    {
    public:
        void setSource (std::atomic<float>* parameter) { excitationParam = parameter; }
        void setProcessor (MorphiumAudioProcessor* p) { processor = p; }
        void setPhase (float newPhase) { phase = newPhase; repaint(); }
        void paint (juce::Graphics&) override;

    private:
        std::atomic<float>* excitationParam = nullptr;
        MorphiumAudioProcessor* processor = nullptr;
        float phase = 0.0f;
    };

    /**
        The "Borato Macro" hero knob. Custom-drawn: a thick gold value ring with
        a glow over a dark track, a metal body with a bold pointer, and A / B / C
        markers that brighten near the current morph zone.
    */
    class MacroKnob : public juce::Slider
    {
    public:
        MacroKnob() : juce::Slider (juce::Slider::RotaryHorizontalVerticalDrag,
                                    juce::Slider::NoTextBox) {}
        void paint (juce::Graphics&) override;
    };

    /**
        A tiny cyan rotary that sits over the scope: the WAVE knob, scanning the
        wavetable morph frames. Custom-drawn in the scope's cyan so it reads as
        part of the CRT rather than a panel knob.
    */
    class WaveKnob : public juce::Slider
    {
    public:
        WaveKnob() : juce::Slider (juce::Slider::RotaryHorizontalVerticalDrag,
                                   juce::Slider::NoTextBox) {}
        void paint (juce::Graphics&) override;
    };

    /**
        A stereo peak level meter. Two thin vertical bars with decay,
        positioned next to the output knob.
    */
    class LevelMeter : public juce::Component, private juce::Timer
    {
    public:
        LevelMeter();
        void setLevels (float left, float right);
        void paint (juce::Graphics&) override;

    private:
        void timerCallback() override;
        float currentLeft = 0.0f, currentRight = 0.0f;
        float peakHoldLeft = 0.0f, peakHoldRight = 0.0f;
        int peakHoldCounterLeft = 0, peakHoldCounterRight = 0;
        static constexpr int peakHoldFrames = 20; // ~667ms at 30Hz
    };

    class MorphiumAudioProcessorEditor : public juce::AudioProcessorEditor,
                                         private juce::Timer
    {
    public:
        explicit MorphiumAudioProcessorEditor (MorphiumAudioProcessor&);
        ~MorphiumAudioProcessorEditor() override;

        void paint (juce::Graphics&) override;
        void paintOverChildren (juce::Graphics&) override;
        void resized() override;

        // Panic affordances: Esc (or double-clicking the chassis) hard-stops
        // every voice — a safety net for notes left stuck by a lost note-off.
        bool keyPressed (const juce::KeyPress&) override;
        void mouseDoubleClick (const juce::MouseEvent&) override;

        /** Maps an excitation type index (0..17) to its SOURCE family (0..5):
            0 IMPACT · 1 FRICTION · 2 AIR · 3 SYNTH · 4 NOISE · 5 TAPE. */
        static int excitationFamily (int index) noexcept;

    private:
        using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;

        // Helper: lay out a control in original SVG (1280x760) coordinates.
        juce::Rectangle<int> svg (float x, float y, float w, float h) const;

        void configureRotary (juce::Slider&);
        void configureVertical (juce::Slider&);

        void timerCallback() override;
        void stepPreset (int direction);
        void refreshPresetDisplay();
        void showPresetMenu();
        void applyMacro (float amount, int mode);
        void updateMacroHighlight (int mode);

        MorphiumAudioProcessor& processorRef;
        MorphiumLookAndFeel lookAndFeel;

        std::unique_ptr<juce::Drawable> panel;   // SVG background

        PresetDisplay presetDisplay;
        WaveDisplay waveDisplay;
        WaveKnob waveKnob;                       // WAVE: wavetable scan position
        std::unique_ptr<SliderAttachment> waveKnobAtt;
        float wavePhase = 0.0f;

        static constexpr int numSources = 6;
        std::array<SourceButton, numSources> sourceButtons;
        std::unique_ptr<juce::ParameterAttachment> excitationAttachment;

        static constexpr int numResonatorModes = 7;
        std::array<ResonatorButton, numResonatorModes> resonatorButtons;
        std::unique_ptr<juce::ParameterAttachment> resonatorAttachment;

        juce::AudioProcessorValueTreeState::ButtonAttachment* getResonatorAttachment (size_t index);

        PresetDisplay excitationDisplay;
        PresetDisplay macroDisplay;

        juce::Slider densitySlider, massSlider, frictionSlider, wearSlider;
        juce::Slider attackSlider, decaySlider, sustainSlider, releaseSlider;
        
        juce::Slider lfoRateSlider, lfoDepthSlider;
        juce::Slider reverbSizeSlider, reverbMixSlider;
        juce::Slider driveSlider;
        
        juce::Slider outputSlider;
        MacroKnob    macroSlider;                // "Borato Macro": morph engine
        LevelMeter   outputMeter;

        std::unique_ptr<SliderAttachment> densityAtt, massAtt, frictionAtt, wearAtt;
        std::unique_ptr<SliderAttachment> attackAtt, decayAtt, sustainAtt, releaseAtt;
        std::unique_ptr<SliderAttachment> lfoRateAtt, lfoDepthAtt;
        std::unique_ptr<SliderAttachment> reverbSizeAtt, reverbMixAtt;
        std::unique_ptr<SliderAttachment> driveAtt;
        std::unique_ptr<SliderAttachment> outputAtt;
        
        std::unique_ptr<SliderAttachment> macroAtt;
        std::unique_ptr<juce::ParameterAttachment> macroModeAtt;

        // Macro highlight: sliders affected by the current macro mode glow
        // briefly when the macro knob is moved.
        std::vector<juce::Slider*> macroAffectedSliders;
        int macroHighlightFrames = 0;
        static constexpr int macroHighlightDuration = 15; // ~500ms at 30Hz

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MorphiumAudioProcessorEditor)
    };
}
