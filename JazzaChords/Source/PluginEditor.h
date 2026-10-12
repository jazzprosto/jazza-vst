#pragma once

#include "PluginProcessor.h"

#include <juce_audio_utils/juce_audio_utils.h>

// Plays a chord on the built-in synth while a tile is held down.
class Auditioner
{
public:
    explicit Auditioner (ChordPlayerProcessor& p) : processor (p) {}
    ~Auditioner() { stop(); }

    void start (const std::vector<int>& notes);
    void stop();

private:
    ChordPlayerProcessor& processor;
    std::vector<int> sounding;
};

// A clickable, draggable chord box used in both the palette and the progression.
class ChordTile : public juce::Component
{
public:
    std::function<void()> onPress, onRelease, onDoubleClick;
    std::function<void()> onRightClick;
    juce::var dragDescription;

    void setText (const juce::String& mainText, const juce::String& subText = {});
    void setColours (juce::Colour background, juce::Colour text);
    void setHighlighted (bool shouldHighlight);

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

private:
    juce::String text, subText;
    juce::Colour background, textColour;
    bool highlighted = false, pressed = false, dragging = false;
};

class PaletteComponent : public juce::Component, public juce::DragAndDropTarget
{
public:
    PaletteComponent (ChordPlayerProcessor&, Auditioner&);

    void refresh();
    std::function<void (ChordEngine::ChordRef)> onAddChord;
    std::function<void (int)> onRemoveProgressionChord;

    void paint (juce::Graphics&) override;
    void resized() override;

    bool isInterestedInDragSource (const SourceDetails&) override;
    void itemDragEnter (const SourceDetails&) override;
    void itemDragExit (const SourceDetails&) override;
    void itemDropped (const SourceDetails&) override;

private:
    ChordPlayerProcessor& processor;
    Auditioner& auditioner;
    std::vector<std::unique_ptr<ChordTile>> tiles; // index = type * 7 + degree
    bool dropHover = false;
};

class ProgressionComponent : public juce::Component, public juce::DragAndDropTarget
{
public:
    ProgressionComponent (ChordPlayerProcessor&, Auditioner&);

    void refresh();
    void setPlayingIndex (int index);

    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;

    bool isInterestedInDragSource (const SourceDetails&) override;
    void itemDragMove (const SourceDetails&) override;
    void itemDragExit (const SourceDetails&) override;
    void itemDropped (const SourceDetails&) override;

private:
    int insertionIndexFor (int x) const;
    void showMenu (int index);

    ChordPlayerProcessor& processor;
    Auditioner& auditioner;
    std::vector<std::unique_ptr<ChordTile>> tiles;
    int playingIndex = -1, insertIndex = -1;
};

// Drag this into FL Studio to drop the whole progression as a MIDI clip.
class DragMidiHandle : public juce::Component
{
public:
    std::function<juce::File()> createFile;

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override { started = false; }
    void mouseDrag (const juce::MouseEvent&) override;

private:
    bool started = false;
};

class ChordPlayerEditor : public juce::AudioProcessorEditor,
                          public juce::DragAndDropContainer,
                          private juce::Timer
{
public:
    explicit ChordPlayerEditor (ChordPlayerProcessor&);
    ~ChordPlayerEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    bool shouldDropFilesWhenDraggedExternally (const juce::DragAndDropTarget::SourceDetails&,
                                               juce::StringArray& files, bool& canMoveFiles) override;

private:
    struct ParamCombo
    {
        juce::Label label;
        juce::ComboBox box;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> attachment;
    };

    void timerCallback() override;
    void refreshAll();
    std::vector<ChordEngine::ChordRef> refsFromDescription (const juce::String& description) const;
    juce::File writeMidiFile (const std::vector<ChordEngine::ChordRef>& progression) const;

    ChordPlayerProcessor& processor;
    juce::LookAndFeel_V4 lookAndFeel;
    Auditioner auditioner;
    juce::Random random;

    std::vector<std::unique_ptr<ParamCombo>> combos;

    juce::Label volumeLabel;
    juce::Slider volumeSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> volumeAttachment;

    juce::ToggleButton bassButton { "Bass" }, syncButton { "Sync to host" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bassAttachment, syncAttachment;

    juce::TextButton playButton { "Play" }, exploreButton { "Explore" }, clearButton { "Clear" };

    juce::MidiKeyboardComponent keyboard;
    juce::Label paletteLabel, progressionLabel;
    PaletteComponent palette;
    ProgressionComponent progression;
    DragMidiHandle dragHandle;

    int lastVersion = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChordPlayerEditor)
};
