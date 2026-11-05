#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <memory>
#include "PluginProcessor.h"

// ---------------------------------------------------------------------------------------------
class UbermenschLookAndFeel : public juce::LookAndFeel_V4
{
public:
    static inline const juce::Colour ink     { 0xff0c0a10 };
    static inline const juce::Colour panel   { 0xff17131f };
    static inline const juce::Colour gold    { 0xffd9a441 };
    static inline const juce::Colour goldDim { 0xff6b5421 };
    static inline const juce::Colour text    { 0xffe9e4f0 };
    static inline const juce::Colour muted   { 0xff7d7689 };

    UbermenschLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool, bool) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool, bool) override;
};

// Big round footswitch-style toggle with a glow driven by the wet envelope
class EngageButton : public juce::ToggleButton
{
public:
    void setGlow (float g) { if (std::abs (g - glow) > 0.004f) { glow = g; repaint(); } }
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;
private:
    float glow = 0.0f;
};

// ---------------------------------------------------------------------------------------------
class UbermenschAudioProcessorEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit UbermenschAudioProcessorEditor (UbermenschAudioProcessor&);
    ~UbermenschAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct Knob
    {
        juce::Slider slider;
        juce::Label  caption;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };

    void timerCallback() override;
    void addKnob (Knob&, const juce::String& paramId, const juce::String& title);
    void updateModeUi();

    UbermenschAudioProcessor& processor;
    UbermenschLookAndFeel lnf;

    Knob attack, decay, gliss, sub, oct, tone, width, mix, level;
    std::array<juce::TextButton, 3> modeButtons;
    juce::ComboBox modeBox;   // hidden; carries the parameter attachment
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> modeAttachment;

    EngageButton engageButton;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> engageAttachment;

    juce::TextButton grabButton { "GRAB" };
    std::unique_ptr<juce::ParameterAttachment> grabAttachment;
    bool grabWasDown = false;

    juce::Rectangle<int> knobRowA, knobRowB, rightPanel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (UbermenschAudioProcessorEditor)
};
