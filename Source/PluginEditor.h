#pragma once

#include "PluginProcessor.h"

#include <juce_audio_utils/juce_audio_utils.h>

// One effect panel: title, on/off switch, an optional selector row and a grid of knobs.
class FxSection : public juce::Component
{
public:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using ComboBoxAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    FxSection (juce::AudioProcessorValueTreeState& state, const juce::String& title,
               juce::Colour accent, const juce::String& enableParamId);

    void addKnob (const juce::String& paramId, const juce::String& name);
    void addChoice (const juce::String& paramId);
    void addToggle (const juce::String& paramId, const juce::String& text);

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct Knob
    {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<SliderAttachment> attachment;
    };

    juce::AudioProcessorValueTreeState& state;
    juce::String title;
    juce::Colour accent;

    juce::ToggleButton enableButton { "On" };
    std::unique_ptr<ButtonAttachment> enableAttachment;

    std::unique_ptr<juce::ComboBox> choice;
    std::unique_ptr<ComboBoxAttachment> choiceAttachment;

    std::unique_ptr<juce::ToggleButton> toggle;
    std::unique_ptr<ButtonAttachment> toggleAttachment;

    std::vector<std::unique_ptr<Knob>> knobs;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FxSection)
};

class TriFXAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit TriFXAudioProcessorEditor (TriFXAudioProcessor&);

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    FxSection distortion, delay, reverb;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TriFXAudioProcessorEditor)
};
