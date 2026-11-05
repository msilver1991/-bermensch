#include "PluginEditor.h"

namespace
{
juce::String titleText() { return juce::String (juce::CharPointer_UTF8 ("\xc3\x9c" "BERMENSCH")); }
}

// ---------------------------------------------------------------------------------------------
UbermenschLookAndFeel::UbermenschLookAndFeel()
{
    setColour (juce::Slider::textBoxTextColourId,       text);
    setColour (juce::Slider::textBoxOutlineColourId,    juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Label::textColourId,               muted);
}

void UbermenschLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                              float startAngle, float endAngle, juce::Slider& s)
{
    const auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (6.0f);
    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre  = bounds.getCentre();
    const float arcR   = radius - 3.0f;
    const bool enabled = s.isEnabled();
    const float angle  = startAngle + pos * (endAngle - startAngle);

    // knob body
    g.setGradientFill (juce::ColourGradient (panel.brighter (0.25f), centre.x, centre.y - radius,
                                             ink, centre.x, centre.y + radius, false));
    g.fillEllipse (centre.x - radius + 6.0f, centre.y - radius + 6.0f, (radius - 6.0f) * 2.0f, (radius - 6.0f) * 2.0f);

    juce::Path track;
    track.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f, startAngle, endAngle, true);
    g.setColour (goldDim.withAlpha (enabled ? 0.45f : 0.2f));
    g.strokePath (track, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    juce::Path value;
    value.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f, startAngle, angle, true);
    g.setColour (enabled ? gold : muted.withAlpha (0.5f));
    g.strokePath (value, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // pointer
    juce::Path pointer;
    const float pr = radius - 9.0f;
    pointer.startNewSubPath (0.0f, -pr * 0.45f);
    pointer.lineTo (0.0f, -pr);
    g.setColour (enabled ? text : muted);
    g.strokePath (pointer, juce::PathStrokeType (2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded),
                  juce::AffineTransform::rotation (angle).translated (centre.x, centre.y));
}

void UbermenschLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down)
{
    const auto r = b.getLocalBounds().toFloat().reduced (1.0f);
    const bool on = b.getToggleState();
    g.setColour (on ? gold : panel.brighter (down ? 0.3f : over ? 0.15f : 0.05f));
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (on ? gold : goldDim.withAlpha (0.6f));
    g.drawRoundedRectangle (r, 4.0f, 1.0f);
}

void UbermenschLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool)
{
    g.setFont (juce::Font (juce::FontOptions (12.5f, juce::Font::bold)));
    g.setColour (b.getToggleState() ? ink : text);
    g.drawText (b.getButtonText(), b.getLocalBounds(), juce::Justification::centred);
}

// ---------------------------------------------------------------------------------------------
void EngageButton::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    using L = UbermenschLookAndFeel;
    const auto b = getLocalBounds().toFloat().reduced (10.0f);
    const float d = juce::jmin (b.getWidth(), b.getHeight());
    const auto c = b.getCentre();
    const auto circle = juce::Rectangle<float> (d, d).withCentre (c);

    if (glow > 0.01f)
    {
        juce::ColourGradient halo (L::gold.withAlpha (0.55f * glow), c.x, c.y,
                                   L::gold.withAlpha (0.0f), c.x, c.y - d * 0.9f, true);
        g.setGradientFill (halo);
        g.fillEllipse (circle.expanded (d * 0.35f));
    }
    g.setGradientFill (juce::ColourGradient (L::panel.brighter (down ? 0.1f : highlighted ? 0.4f : 0.3f), c.x, c.y - d * 0.5f,
                                             L::ink, c.x, c.y + d * 0.5f, false));
    g.fillEllipse (circle);
    g.setColour (getToggleState() ? L::gold : L::goldDim);
    g.drawEllipse (circle.reduced (1.5f), 3.0f);

    g.setColour (getToggleState() ? L::gold : L::muted);
    g.setFont (juce::Font (juce::FontOptions (15.0f, juce::Font::bold)));
    g.drawText ("ENGAGE", circle.toNearestInt(), juce::Justification::centred);
}

// ---------------------------------------------------------------------------------------------
UbermenschAudioProcessorEditor::UbermenschAudioProcessorEditor (UbermenschAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    setLookAndFeel (&lnf);
    setSize (760, 420);

    addKnob (attack, "attack", "ATTACK");
    addKnob (decay,  "decay",  "DECAY");
    addKnob (gliss,  "gliss",  "GLISS");
    addKnob (sub,    "sub",    "SUB");
    addKnob (oct,    "oct",    "OCTAVE");
    addKnob (tone,   "tone",   "TONE");
    addKnob (width,  "width",  "WIDTH");
    addKnob (mix,    "mix",    "BLEND");
    addKnob (level,  "level",  "LEVEL");

    // mode selector
    const char* names[] = { "FREEZE", "SUSTAIN", "SYNTH" };
    addChildComponent (modeBox);
    modeBox.addItemList (juce::StringArray { "Freeze", "Sustain", "Synth" }, 1);
    modeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (processor.apvts, "mode", modeBox);
    for (int i = 0; i < 3; ++i)
    {
        auto& b = modeButtons[(size_t) i];
        b.setButtonText (names[i]);
        b.setClickingTogglesState (false);
        b.onClick = [this, i] { modeBox.setSelectedItemIndex (i, juce::sendNotificationSync); };
        addAndMakeVisible (b);
    }
    modeBox.onChange = [this] { updateModeUi(); };

    addAndMakeVisible (engageButton);
    engageAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (processor.apvts, "engage", engageButton);

    // momentary GRAB
    addAndMakeVisible (grabButton);
    grabAttachment = std::make_unique<juce::ParameterAttachment> (*processor.apvts.getParameter ("grab"), [] (float) {}, nullptr);
    grabButton.onStateChange = [this]
    {
        const bool down = grabButton.isDown();
        if (down == grabWasDown) return;
        grabWasDown = down;
        if (down) grabAttachment->beginGesture();
        grabAttachment->setValueAsPartOfGesture (down ? 1.0f : 0.0f);
        if (! down) grabAttachment->endGesture();
    };
    grabButton.setTooltip ("Take a new capture while engaged (glides to it according to Gliss)");

    updateModeUi();
    startTimerHz (30);
}

UbermenschAudioProcessorEditor::~UbermenschAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void UbermenschAudioProcessorEditor::addKnob (Knob& k, const juce::String& paramId, const juce::String& title)
{
    k.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    k.slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
    k.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 90, 18);
    k.slider.setPopupDisplayEnabled (false, false, nullptr);
    addAndMakeVisible (k.slider);
    k.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (processor.apvts, paramId, k.slider);

    k.caption.setText (title, juce::dontSendNotification);
    k.caption.setJustificationType (juce::Justification::centred);
    k.caption.setFont (juce::Font (juce::FontOptions (11.5f, juce::Font::bold)));
    k.caption.setColour (juce::Label::textColourId, UbermenschLookAndFeel::muted);
    addAndMakeVisible (k.caption);
}

void UbermenschAudioProcessorEditor::updateModeUi()
{
    const int m = juce::jmax (0, modeBox.getSelectedItemIndex());
    for (int i = 0; i < 3; ++i)
        modeButtons[(size_t) i].setToggleState (i == m, juce::dontSendNotification);

    const bool synth = (m == 2);
    sub.slider.setEnabled (synth);
    oct.slider.setEnabled (synth);
    sub.caption.setAlpha (synth ? 1.0f : 0.4f);
    oct.caption.setAlpha (synth ? 1.0f : 0.4f);
    repaint();
}

void UbermenschAudioProcessorEditor::timerCallback()
{
    engageButton.setGlow (processor.envelopeMeter.load());
    // the host may change "mode" while the editor is open
    const int host = juce::roundToInt (processor.apvts.getRawParameterValue ("mode")->load());
    if (host != modeBox.getSelectedItemIndex())
        modeBox.setSelectedItemIndex (host, juce::dontSendNotification), updateModeUi();
}

void UbermenschAudioProcessorEditor::paint (juce::Graphics& g)
{
    using L = UbermenschLookAndFeel;
    g.setGradientFill (juce::ColourGradient (L::panel, 0.0f, 0.0f, L::ink, 0.0f, (float) getHeight(), false));
    g.fillAll();

    // title
    g.setColour (L::gold);
    g.setFont (juce::Font (juce::FontOptions (34.0f, juce::Font::bold)));
    g.drawText (titleText(), 30, 18, 380, 40, juce::Justification::centredLeft);
    g.setColour (L::muted);
    g.setFont (juce::Font (juce::FontOptions (12.0f)));
    g.drawText ("FREEZE  /  SUSTAIN  /  SYNTH ENGINE", 32, 56, 380, 18, juce::Justification::centredLeft);

    g.setColour (L::goldDim.withAlpha (0.5f));
    g.drawHorizontalLine (92, 30.0f, (float) getWidth() - 30.0f);
    g.drawVerticalLine (rightPanel.getX() - 10, 110.0f, (float) getHeight() - 30.0f);
}

void UbermenschAudioProcessorEditor::resized()
{
    auto area = getLocalBounds();

    // mode buttons (top right)
    auto top = area.removeFromTop (92).reduced (30, 0);
    auto modeArea = top.removeFromRight (300).withTrimmedTop (28).withHeight (32);
    const int bw = modeArea.getWidth() / 3;
    for (int i = 0; i < 3; ++i)
        modeButtons[(size_t) i].setBounds (modeArea.removeFromLeft (bw).reduced (3, 0));

    area.reduce (30, 14);
    rightPanel = area.removeFromRight (190);
    area.removeFromRight (20);

    auto place = [] (Knob& k, juce::Rectangle<int> cell)
    {
        k.caption.setBounds (cell.removeFromTop (16));
        k.slider.setBounds (cell);
    };

    const int rowH = area.getHeight() / 2;
    knobRowA = area.removeFromTop (rowH);
    knobRowB = area;

    const int cw = knobRowA.getWidth() / 5;
    Knob* rowA[] = { &attack, &decay, &gliss, &sub, &oct };
    for (int i = 0; i < 5; ++i) place (*rowA[i], knobRowA.removeFromLeft (cw).reduced (4, 2));
    Knob* rowB[] = { &tone, &width, &mix, &level };
    for (int i = 0; i < 4; ++i) place (*rowB[i], knobRowB.removeFromLeft (cw).reduced (4, 2));

    // right panel: footswitch + grab
    auto rp = rightPanel;
    grabButton.setBounds (rp.removeFromBottom (34).reduced (30, 0));
    rp.removeFromBottom (14);
    engageButton.setBounds (rp.withSizeKeepingCentre (juce::jmin (rp.getWidth(), rp.getHeight()), juce::jmin (rp.getWidth(), rp.getHeight())));
}
