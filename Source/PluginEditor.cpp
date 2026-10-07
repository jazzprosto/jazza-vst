#include "PluginEditor.h"

namespace Palette
{
    const juce::Colour background { 0xff15171c };
    const juce::Colour panel      { 0xff20232a };
    const juce::Colour track      { 0xff3a3e48 };
    const juce::Colour text       { 0xffe6e8ee };
    const juce::Colour dimText    { 0xff8b90a0 };
}

//==============================================================================
FxSection::FxSection (juce::AudioProcessorValueTreeState& s, const juce::String& sectionTitle,
                      juce::Colour accentColour, const juce::String& enableParamId)
    : state (s), title (sectionTitle), accent (accentColour)
{
    enableButton.setColour (juce::ToggleButton::tickColourId, accent);
    enableButton.setColour (juce::ToggleButton::textColourId, Palette::text);
    addAndMakeVisible (enableButton);
    enableAttachment = std::make_unique<ButtonAttachment> (state, enableParamId, enableButton);
}

void FxSection::addKnob (const juce::String& paramId, const juce::String& name)
{
    auto knob = std::make_unique<Knob>();

    knob->slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    knob->slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 90, 20);
    knob->slider.setColour (juce::Slider::rotarySliderFillColourId, accent);
    knob->slider.setColour (juce::Slider::rotarySliderOutlineColourId, Palette::track);
    knob->slider.setColour (juce::Slider::thumbColourId, accent);
    knob->slider.setColour (juce::Slider::textBoxTextColourId, Palette::text);
    knob->slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);

    knob->label.setText (name, juce::dontSendNotification);
    knob->label.setJustificationType (juce::Justification::centred);
    knob->label.setColour (juce::Label::textColourId, Palette::dimText);

    addAndMakeVisible (knob->slider);
    addAndMakeVisible (knob->label);
    knob->attachment = std::make_unique<SliderAttachment> (state, paramId, knob->slider);

    knobs.push_back (std::move (knob));
}

void FxSection::addChoice (const juce::String& paramId)
{
    choice = std::make_unique<juce::ComboBox>();

    if (auto* param = dynamic_cast<juce::AudioParameterChoice*> (state.getParameter (paramId)))
        choice->addItemList (param->choices, 1);

    choice->setColour (juce::ComboBox::backgroundColourId, Palette::background);
    choice->setColour (juce::ComboBox::outlineColourId, Palette::track);
    choice->setColour (juce::ComboBox::textColourId, Palette::text);
    choice->setColour (juce::ComboBox::arrowColourId, accent);

    addAndMakeVisible (*choice);
    choiceAttachment = std::make_unique<ComboBoxAttachment> (state, paramId, *choice);
}

void FxSection::addToggle (const juce::String& paramId, const juce::String& text)
{
    toggle = std::make_unique<juce::ToggleButton> (text);
    toggle->setColour (juce::ToggleButton::tickColourId, accent);
    toggle->setColour (juce::ToggleButton::textColourId, Palette::text);

    addAndMakeVisible (*toggle);
    toggleAttachment = std::make_unique<ButtonAttachment> (state, paramId, *toggle);
}

void FxSection::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    g.setColour (Palette::panel);
    g.fillRoundedRectangle (bounds, 10.0f);

    g.setColour (accent);
    g.fillRoundedRectangle (bounds.removeFromTop (4.0f).reduced (10.0f, 0.0f), 2.0f);

    g.setColour (Palette::text);
    g.setFont (juce::Font (juce::FontOptions (16.0f, juce::Font::bold)));
    g.drawText (title, getLocalBounds().reduced (14, 12).removeFromTop (28), juce::Justification::centredLeft);
}

void FxSection::resized()
{
    auto area = getLocalBounds().reduced (14, 12);

    auto header = area.removeFromTop (28);
    enableButton.setBounds (header.removeFromRight (60));

    area.removeFromTop (8);
    auto extraRow = area.removeFromTop (26);

    if (choice != nullptr)
        choice->setBounds (extraRow);
    else if (toggle != nullptr)
        toggle->setBounds (extraRow);

    area.removeFromTop (8);

    constexpr int columns = 2;
    const int rows = juce::jmax (1, ((int) knobs.size() + columns - 1) / columns);
    const int cellWidth = area.getWidth() / columns;
    const int cellHeight = area.getHeight() / rows;

    for (int i = 0; i < (int) knobs.size(); ++i)
    {
        juce::Rectangle<int> cell (area.getX() + (i % columns) * cellWidth,
                                   area.getY() + (i / columns) * cellHeight,
                                   cellWidth, cellHeight);

        knobs[(size_t) i]->label.setBounds (cell.removeFromTop (18));
        knobs[(size_t) i]->slider.setBounds (cell.reduced (4, 2));
    }
}

//==============================================================================
TriFXAudioProcessorEditor::TriFXAudioProcessorEditor (TriFXAudioProcessor& p)
    : AudioProcessorEditor (&p),
      distortion (p.apvts, "DISTORTION", juce::Colour (0xffff7a45), ParamIDs::distOn),
      delay      (p.apvts, "DELAY",      juce::Colour (0xff3fc1e0), ParamIDs::delayOn),
      reverb     (p.apvts, "REVERB",     juce::Colour (0xffa27bff), ParamIDs::revOn)
{
    distortion.addChoice (ParamIDs::distType);
    distortion.addKnob (ParamIDs::distDrive,  "Drive");
    distortion.addKnob (ParamIDs::distTone,   "Tone");
    distortion.addKnob (ParamIDs::distOutput, "Output");
    distortion.addKnob (ParamIDs::distMix,    "Mix");

    delay.addToggle (ParamIDs::delayPingPong, "Ping-Pong");
    delay.addKnob (ParamIDs::delayTime,     "Time");
    delay.addKnob (ParamIDs::delayFeedback, "Feedback");
    delay.addKnob (ParamIDs::delayTone,     "Tone");
    delay.addKnob (ParamIDs::delayMix,      "Mix");

    reverb.addKnob (ParamIDs::revSize,  "Size");
    reverb.addKnob (ParamIDs::revDamp,  "Damping");
    reverb.addKnob (ParamIDs::revWidth, "Width");
    reverb.addKnob (ParamIDs::revMix,   "Mix");

    addAndMakeVisible (distortion);
    addAndMakeVisible (delay);
    addAndMakeVisible (reverb);

    setSize (800, 420);
}

void TriFXAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (Palette::background);

    auto header = getLocalBounds().reduced (16, 0).removeFromTop (48);

    g.setColour (Palette::text);
    g.setFont (juce::Font (juce::FontOptions (22.0f, juce::Font::bold)));
    g.drawText ("TriFX", header, juce::Justification::centredLeft);

    g.setColour (Palette::dimText);
    g.setFont (juce::Font (juce::FontOptions (13.0f)));
    g.drawText ("Distortion  >  Delay  >  Reverb", header, juce::Justification::centredRight);
}

void TriFXAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (16);
    area.removeFromTop (40);

    constexpr int gap = 12;
    const int width = (area.getWidth() - 2 * gap) / 3;

    distortion.setBounds (area.removeFromLeft (width));
    area.removeFromLeft (gap);
    delay.setBounds (area.removeFromLeft (width));
    area.removeFromLeft (gap);
    reverb.setBounds (area);
}
