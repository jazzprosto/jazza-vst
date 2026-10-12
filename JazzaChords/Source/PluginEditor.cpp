#include "PluginEditor.h"

namespace Palette
{
    const juce::Colour background { 0xff2d3322 };
    const juce::Colour panel      { 0xff444d30 };
    const juce::Colour control    { 0xff3a4229 };
    const juce::Colour button     { 0xff5a6640 };
    const juce::Colour tile       { 0xffe2dfc4 };
    const juce::Colour accent     { 0xffe8b923 };
    const juce::Colour playing    { 0xffff6a2a };
    const juce::Colour darkText   { 0xff2b2b22 };
    const juce::Colour text       { 0xffeeecd8 };
    const juce::Colour dimText    { 0xffb3b394 };
}

namespace
{
    juce::String lengthLabel (int beats)
    {
        switch (beats)
        {
            case 1:  return "1 beat";
            case 2:  return "2 beats";
            case 4:  return "1 bar";
            case 8:  return "2 bars";
            default: return juce::String (beats) + " beats";
        }
    }

    juce::Font font (float height, bool bold = false)
    {
        return juce::Font (juce::FontOptions (height, bold ? juce::Font::bold : juce::Font::plain));
    }
}

//==============================================================================
void Auditioner::start (const std::vector<int>& notes)
{
    stop();
    sounding = notes;

    for (auto note : sounding)
        processor.keyboardState.noteOn (1, note, 0.8f);
}

void Auditioner::stop()
{
    for (auto note : sounding)
        processor.keyboardState.noteOff (1, note, 0.0f);

    sounding.clear();
}

//==============================================================================
void ChordTile::setText (const juce::String& mainText, const juce::String& secondaryText)
{
    text = mainText;
    subText = secondaryText;
    repaint();
}

void ChordTile::setColours (juce::Colour newBackground, juce::Colour newText)
{
    background = newBackground;
    textColour = newText;
    repaint();
}

void ChordTile::setHighlighted (bool shouldHighlight)
{
    if (highlighted != shouldHighlight)
    {
        highlighted = shouldHighlight;
        repaint();
    }
}

void ChordTile::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat().reduced (2.0f);

    g.setColour (pressed ? background.darker (0.2f) : background);
    g.fillRoundedRectangle (bounds, 5.0f);

    if (highlighted)
    {
        g.setColour (Palette::playing);
        g.drawRoundedRectangle (bounds.reduced (1.5f), 5.0f, 3.0f);
    }

    g.setColour (textColour);

    if (subText.isEmpty())
    {
        g.setFont (font (15.0f, true));
        g.drawFittedText (text, bounds.toNearestInt(), juce::Justification::centred, 1);
    }
    else
    {
        auto area = bounds.toNearestInt();
        g.setFont (font (15.0f, true));
        g.drawFittedText (text, area.removeFromTop (area.getHeight() * 3 / 5), juce::Justification::centredBottom, 1);
        g.setFont (font (11.0f));
        g.drawFittedText (subText, area, juce::Justification::centredTop, 1);
    }
}

void ChordTile::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu())
    {
        if (onRightClick)
            onRightClick();

        return;
    }

    dragging = false;
    pressed = true;

    if (onPress)
        onPress();

    repaint();
}

void ChordTile::mouseUp (const juce::MouseEvent&)
{
    if (pressed && onRelease)
        onRelease();

    pressed = false;
    dragging = false;
    repaint();
}

void ChordTile::mouseDrag (const juce::MouseEvent& e)
{
    if (dragging || ! pressed || dragDescription.isVoid() || e.getDistanceFromDragStart() < 6)
        return;

    dragging = true;
    pressed = false;

    if (onRelease)
        onRelease();

    repaint();

    if (auto* container = juce::DragAndDropContainer::findParentDragContainerFor (this))
        container->startDragging (dragDescription, this);
}

void ChordTile::mouseDoubleClick (const juce::MouseEvent& e)
{
    if (! e.mods.isPopupMenu() && onDoubleClick)
        onDoubleClick();
}

//==============================================================================
PaletteComponent::PaletteComponent (ChordPlayerProcessor& p, Auditioner& a)
    : processor (p), auditioner (a)
{
    for (int type = 0; type < ChordEngine::numChordTypes; ++type)
    {
        for (int degree = 0; degree < ChordEngine::numDegrees; ++degree)
        {
            auto tile = std::make_unique<ChordTile>();
            tile->setColours (Palette::tile, Palette::darkText);
            tile->dragDescription = "palette:" + juce::String (degree) + ":" + juce::String (type);
            tile->onPress = [this, degree, type] { auditioner.start (processor.getChordNotes (degree, type)); };
            tile->onRelease = [this] { auditioner.stop(); };
            tile->onDoubleClick = [this, degree, type]
            {
                if (onAddChord)
                    onAddChord ({ degree, type, 0 });
            };

            addAndMakeVisible (*tile);
            tiles.push_back (std::move (tile));
        }
    }
}

void PaletteComponent::refresh()
{
    for (int type = 0; type < ChordEngine::numChordTypes; ++type)
    {
        for (int degree = 0; degree < ChordEngine::numDegrees; ++degree)
        {
            const auto chord = processor.getChord (degree, type);
            auto& tile = *tiles[(size_t) (type * ChordEngine::numDegrees + degree)];
            tile.setText (chord.name);
            tile.setVisible (chord.valid);
        }
    }
}

void PaletteComponent::paint (juce::Graphics& g)
{
    g.setColour (dropHover ? Palette::panel.brighter (0.15f) : Palette::panel);
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 8.0f);

    const auto names = ChordEngine::getChordTypeNames();
    auto area = getLocalBounds().reduced (4);
    const int rowHeight = area.getHeight() / ChordEngine::numChordTypes;

    g.setColour (Palette::dimText);
    g.setFont (font (12.0f));

    for (int type = 0; type < ChordEngine::numChordTypes; ++type)
        g.drawText (names[type], area.getX() + 4, area.getY() + type * rowHeight, 40, rowHeight,
                    juce::Justification::centredLeft);

    if (dropHover)
    {
        g.setColour (Palette::text);
        g.setFont (font (13.0f, true));
        g.drawText ("Drop to remove from progression", getLocalBounds().removeFromBottom (18),
                    juce::Justification::centred);
    }
}

void PaletteComponent::resized()
{
    auto area = getLocalBounds().reduced (4);
    area.removeFromLeft (44);

    const int cellWidth = area.getWidth() / ChordEngine::numDegrees;
    const int cellHeight = area.getHeight() / ChordEngine::numChordTypes;

    for (int type = 0; type < ChordEngine::numChordTypes; ++type)
        for (int degree = 0; degree < ChordEngine::numDegrees; ++degree)
            tiles[(size_t) (type * ChordEngine::numDegrees + degree)]->setBounds (area.getX() + degree * cellWidth,
                                                                                 area.getY() + type * cellHeight,
                                                                                 cellWidth, cellHeight);
}

bool PaletteComponent::isInterestedInDragSource (const SourceDetails& details)
{
    return details.description.toString().startsWith ("prog:");
}

void PaletteComponent::itemDragEnter (const SourceDetails&)
{
    dropHover = true;
    repaint();
}

void PaletteComponent::itemDragExit (const SourceDetails&)
{
    dropHover = false;
    repaint();
}

void PaletteComponent::itemDropped (const SourceDetails& details)
{
    dropHover = false;
    repaint();

    if (onRemoveProgressionChord)
        onRemoveProgressionChord (details.description.toString().fromFirstOccurrenceOf (":", false, false).getIntValue());
}

//==============================================================================
ProgressionComponent::ProgressionComponent (ChordPlayerProcessor& p, Auditioner& a)
    : processor (p), auditioner (a)
{
}

void ProgressionComponent::refresh()
{
    auditioner.stop();
    tiles.clear();

    const auto progression = processor.getProgression();

    for (size_t i = 0; i < progression.size(); ++i)
    {
        const auto ref = progression[i];
        const auto chord = processor.getChord (ref.degree, ref.type);
        const int index = (int) i;

        auto tile = std::make_unique<ChordTile>();
        tile->setText (chord.valid ? chord.name : juce::String ("-"), ref.beats > 0 ? lengthLabel (ref.beats) : juce::String());
        tile->setColours (Palette::accent, Palette::darkText);
        tile->dragDescription = "prog:" + juce::String (index);
        tile->onPress = [this, ref] { auditioner.start (processor.getChordNotes (ref.degree, ref.type)); };
        tile->onRelease = [this] { auditioner.stop(); };
        tile->onRightClick = [this, index] { showMenu (index); };
        tile->setHighlighted (index == playingIndex);

        addAndMakeVisible (*tile);
        tiles.push_back (std::move (tile));
    }

    resized();
    repaint();
}

void ProgressionComponent::setPlayingIndex (int index)
{
    if (index == playingIndex)
        return;

    playingIndex = index;

    for (size_t i = 0; i < tiles.size(); ++i)
        tiles[i]->setHighlighted ((int) i == playingIndex);
}

void ProgressionComponent::paint (juce::Graphics& g)
{
    g.setColour (Palette::panel);
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 8.0f);

    if (tiles.empty())
    {
        g.setColour (Palette::dimText);
        g.setFont (font (14.0f));
        g.drawText ("Drag and drop chords here (or double-click a chord above)", getLocalBounds(),
                    juce::Justification::centred);
    }
}

void ProgressionComponent::paintOverChildren (juce::Graphics& g)
{
    if (insertIndex < 0)
        return;

    int x = 6;

    if (insertIndex < (int) tiles.size())
        x = tiles[(size_t) insertIndex]->getX();
    else if (! tiles.empty())
        x = tiles.back()->getRight();

    g.setColour (Palette::playing);
    g.fillRect (x - 2, 4, 4, getHeight() - 8);
}

void ProgressionComponent::resized()
{
    if (tiles.empty())
        return;

    auto area = getLocalBounds().reduced (6);
    const int tileWidth = juce::jmin (110, area.getWidth() / (int) tiles.size());

    for (auto& tile : tiles)
        tile->setBounds (area.removeFromLeft (tileWidth));
}

int ProgressionComponent::insertionIndexFor (int x) const
{
    for (size_t i = 0; i < tiles.size(); ++i)
        if (x < tiles[i]->getBounds().getCentreX())
            return (int) i;

    return (int) tiles.size();
}

bool ProgressionComponent::isInterestedInDragSource (const SourceDetails& details)
{
    const auto description = details.description.toString();
    return description.startsWith ("palette:") || description.startsWith ("prog:");
}

void ProgressionComponent::itemDragMove (const SourceDetails& details)
{
    const int index = insertionIndexFor (details.localPosition.x);

    if (index != insertIndex)
    {
        insertIndex = index;
        repaint();
    }
}

void ProgressionComponent::itemDragExit (const SourceDetails&)
{
    insertIndex = -1;
    repaint();
}

void ProgressionComponent::itemDropped (const SourceDetails& details)
{
    const auto parts = juce::StringArray::fromTokens (details.description.toString(), ":", "");
    auto progression = processor.getProgression();
    int target = juce::jlimit (0, (int) progression.size(), insertionIndexFor (details.localPosition.x));

    insertIndex = -1;
    repaint();

    if (parts[0] == "palette" && parts.size() == 3)
    {
        progression.insert (progression.begin() + target, ChordEngine::ChordRef { parts[1].getIntValue(), parts[2].getIntValue(), 0 });
    }
    else if (parts[0] == "prog" && parts.size() == 2)
    {
        const int source = parts[1].getIntValue();

        if (! juce::isPositiveAndBelow (source, (int) progression.size()))
            return;

        const auto chord = progression[(size_t) source];
        progression.erase (progression.begin() + source);

        if (source < target)
            --target;

        progression.insert (progression.begin() + target, chord);
    }
    else
    {
        return;
    }

    processor.setProgression (progression);
}

void ProgressionComponent::showMenu (int index)
{
    const auto progression = processor.getProgression();

    if (! juce::isPositiveAndBelow (index, (int) progression.size()))
        return;

    const auto ref = progression[(size_t) index];

    juce::PopupMenu lengthMenu;
    lengthMenu.addItem (10, "Default", true, ref.beats == 0);

    for (int i = 0; i < 4; ++i)
    {
        const int beats = (int) ChordEngine::getLengthBeats (i);
        lengthMenu.addItem (11 + i, lengthLabel (beats), true, ref.beats == beats);
    }

    juce::PopupMenu typeMenu;
    const auto typeNames = ChordEngine::getChordTypeNames();

    for (int type = 0; type < ChordEngine::numChordTypes; ++type)
        typeMenu.addItem (20 + type, typeNames[type], processor.getChord (ref.degree, type).valid, ref.type == type);

    juce::PopupMenu menu;
    menu.addItem (1, "Remove");
    menu.addItem (2, "Duplicate");
    menu.addSubMenu ("Length", lengthMenu);
    menu.addSubMenu ("Chord type", typeMenu);

    juce::Component::SafePointer<ProgressionComponent> safeThis (this);

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (tiles[(size_t) index].get()),
                        [safeThis, index] (int result)
    {
        if (safeThis == nullptr || result == 0)
            return;

        auto& processor = safeThis->processor;
        auto chords = processor.getProgression();

        if (! juce::isPositiveAndBelow (index, (int) chords.size()))
            return;

        if (result == 1)
            chords.erase (chords.begin() + index);
        else if (result == 2)
            chords.insert (chords.begin() + index + 1, chords[(size_t) index]);
        else if (result == 10)
            chords[(size_t) index].beats = 0;
        else if (result >= 11 && result <= 14)
            chords[(size_t) index].beats = (int) ChordEngine::getLengthBeats (result - 11);
        else if (result >= 20)
            chords[(size_t) index].type = result - 20;

        processor.setProgression (chords);
    });
}

//==============================================================================
void DragMidiHandle::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat().reduced (1.0f);

    g.setColour (isMouseOver() ? Palette::button.brighter (0.15f) : Palette::button);
    g.fillRoundedRectangle (bounds, 8.0f);

    g.setColour (Palette::accent);
    g.drawRoundedRectangle (bounds, 8.0f, 1.5f);

    auto area = getLocalBounds().reduced (6);
    g.setColour (Palette::text);
    g.setFont (font (15.0f, true));
    g.drawText ("Drag MIDI", area.removeFromTop (area.getHeight() / 2), juce::Justification::centredBottom);
    g.setColour (Palette::dimText);
    g.setFont (font (11.0f));
    g.drawText ("drop into FL Studio", area, juce::Justification::centredTop);
}

void DragMidiHandle::mouseDrag (const juce::MouseEvent& e)
{
    if (started || e.getDistanceFromDragStart() < 4 || ! createFile)
        return;

    started = true;
    const auto file = createFile();

    if (file.existsAsFile())
        juce::DragAndDropContainer::performExternalDragDropOfFiles (juce::StringArray (file.getFullPathName()), false, this);
}

//==============================================================================
ChordPlayerEditor::ChordPlayerEditor (ChordPlayerProcessor& p)
    : AudioProcessorEditor (&p),
      processor (p),
      auditioner (p),
      keyboard (p.keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard),
      palette (p, auditioner),
      progression (p, auditioner)
{
    lookAndFeel.setColour (juce::ComboBox::backgroundColourId, Palette::control);
    lookAndFeel.setColour (juce::ComboBox::outlineColourId, Palette::button);
    lookAndFeel.setColour (juce::ComboBox::textColourId, Palette::text);
    lookAndFeel.setColour (juce::ComboBox::arrowColourId, Palette::accent);
    lookAndFeel.setColour (juce::PopupMenu::backgroundColourId, Palette::control);
    lookAndFeel.setColour (juce::PopupMenu::textColourId, Palette::text);
    lookAndFeel.setColour (juce::PopupMenu::highlightedBackgroundColourId, Palette::accent);
    lookAndFeel.setColour (juce::PopupMenu::highlightedTextColourId, Palette::darkText);
    lookAndFeel.setColour (juce::TextButton::buttonColourId, Palette::button);
    lookAndFeel.setColour (juce::TextButton::buttonOnColourId, Palette::accent);
    lookAndFeel.setColour (juce::TextButton::textColourOffId, Palette::text);
    lookAndFeel.setColour (juce::TextButton::textColourOnId, Palette::darkText);
    lookAndFeel.setColour (juce::ToggleButton::textColourId, Palette::text);
    lookAndFeel.setColour (juce::ToggleButton::tickColourId, Palette::accent);
    lookAndFeel.setColour (juce::ToggleButton::tickDisabledColourId, Palette::dimText);
    lookAndFeel.setColour (juce::Label::textColourId, Palette::dimText);
    lookAndFeel.setColour (juce::Slider::trackColourId, Palette::accent);
    lookAndFeel.setColour (juce::Slider::backgroundColourId, Palette::control);
    lookAndFeel.setColour (juce::Slider::thumbColourId, Palette::tile);
    setLookAndFeel (&lookAndFeel);

    const std::pair<const char*, const char*> comboParams[] = {
        { ParamIDs::sound, "Sound" },          { ParamIDs::style, "Style" },     { ParamIDs::key, "Key" },
        { ParamIDs::scale, "Scale" },          { ParamIDs::length, "Chord length" },
        { ParamIDs::voicing, "Voicing" },      { ParamIDs::octave, "Octave" },
    };

    for (const auto& [id, name] : comboParams)
    {
        auto combo = std::make_unique<ParamCombo>();
        combo->label.setText (name, juce::dontSendNotification);
        combo->label.setFont (font (12.0f));

        if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (processor.apvts.getParameter (id)))
            combo->box.addItemList (choice->choices, 1);

        addAndMakeVisible (combo->label);
        addAndMakeVisible (combo->box);
        combo->attachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (processor.apvts, id, combo->box);
        combos.push_back (std::move (combo));
    }

    volumeLabel.setText ("Volume", juce::dontSendNotification);
    volumeLabel.setFont (font (12.0f));
    volumeSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    volumeSlider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    addAndMakeVisible (volumeLabel);
    addAndMakeVisible (volumeSlider);
    volumeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (processor.apvts, ParamIDs::volume, volumeSlider);

    addAndMakeVisible (bassButton);
    addAndMakeVisible (syncButton);
    bassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (processor.apvts, ParamIDs::bass, bassButton);
    syncAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (processor.apvts, ParamIDs::hostSync, syncButton);

    playButton.onClick = [this] { processor.setInternalPlaying (! processor.isInternalPlaying()); };
    exploreButton.onClick = [this]
    {
        const auto settings = processor.getSettings();
        processor.setProgression (ChordEngine::randomProgression (random, settings.key, settings.scale));
    };
    clearButton.onClick = [this] { processor.setProgression ({}); };

    addAndMakeVisible (playButton);
    addAndMakeVisible (exploreButton);
    addAndMakeVisible (clearButton);

    keyboard.setAvailableRange (24, 96);
    keyboard.setColour (juce::MidiKeyboardComponent::keyDownOverlayColourId, Palette::accent);
    keyboard.setColour (juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId, Palette::accent.withAlpha (0.3f));
    addAndMakeVisible (keyboard);

    paletteLabel.setFont (font (13.0f, true));
    progressionLabel.setFont (font (13.0f, true));
    progressionLabel.setText ("Chord progression  -  drag to reorder, right-click to edit, drag a chord out to FL",
                              juce::dontSendNotification);
    addAndMakeVisible (paletteLabel);
    addAndMakeVisible (progressionLabel);

    palette.onAddChord = [this] (ChordEngine::ChordRef ref)
    {
        auto chords = processor.getProgression();
        chords.push_back (ref);
        processor.setProgression (chords);
    };

    palette.onRemoveProgressionChord = [this] (int index)
    {
        auto chords = processor.getProgression();

        if (juce::isPositiveAndBelow (index, (int) chords.size()))
        {
            chords.erase (chords.begin() + index);
            processor.setProgression (chords);
        }
    };

    addAndMakeVisible (palette);
    addAndMakeVisible (progression);

    dragHandle.createFile = [this] { return writeMidiFile (processor.getProgression()); };
    addAndMakeVisible (dragHandle);

    setSize (960, 530);
    refreshAll();
    startTimerHz (30);
}

ChordPlayerEditor::~ChordPlayerEditor()
{
    stopTimer();
    auditioner.stop();
    setLookAndFeel (nullptr);
}

void ChordPlayerEditor::paint (juce::Graphics& g)
{
    g.fillAll (Palette::background);

    auto header = getLocalBounds().reduced (14).removeFromTop (34);
    g.setColour (Palette::accent);
    g.setFont (font (22.0f, true));
    g.drawText ("Jazza Chords", header, juce::Justification::centredLeft);
}

void ChordPlayerEditor::resized()
{
    auto area = getLocalBounds().reduced (14);

    auto header = area.removeFromTop (34);
    clearButton.setBounds (header.removeFromRight (70).reduced (0, 2));
    header.removeFromRight (6);
    exploreButton.setBounds (header.removeFromRight (80).reduced (0, 2));
    header.removeFromRight (6);
    playButton.setBounds (header.removeFromRight (80).reduced (0, 2));
    header.removeFromRight (16);
    syncButton.setBounds (header.removeFromRight (120));
    bassButton.setBounds (header.removeFromRight (70));

    area.removeFromTop (8);
    auto controls = area.removeFromTop (46);
    const int widths[] = { 100, 110, 64, 140, 90, 84, 64 };

    for (size_t i = 0; i < combos.size(); ++i)
    {
        auto column = controls.removeFromLeft (widths[i]);
        combos[i]->label.setBounds (column.removeFromTop (18));
        combos[i]->box.setBounds (column.removeFromTop (26));
        controls.removeFromLeft (8);
    }

    auto volumeColumn = controls.removeFromLeft (juce::jmin (200, controls.getWidth()));
    volumeLabel.setBounds (volumeColumn.removeFromTop (18));
    volumeSlider.setBounds (volumeColumn.removeFromTop (26));

    area.removeFromTop (10);
    keyboard.setBounds (area.removeFromTop (64));
    keyboard.setKeyWidth ((float) keyboard.getWidth() / 43.0f); // 43 white keys from C1 to C7

    area.removeFromTop (8);
    paletteLabel.setBounds (area.removeFromTop (20));
    palette.setBounds (area.removeFromTop (5 * 40 + 8));

    area.removeFromTop (8);
    progressionLabel.setBounds (area.removeFromTop (20));

    auto row = area.removeFromTop (72);
    dragHandle.setBounds (row.removeFromRight (130));
    row.removeFromRight (8);
    progression.setBounds (row);
}

void ChordPlayerEditor::timerCallback()
{
    if (processor.getSequenceVersion() != lastVersion)
        refreshAll();

    progression.setPlayingIndex (processor.getCurrentChordIndex());
    playButton.setButtonText (processor.isInternalPlaying() ? "Stop" : "Play");
}

void ChordPlayerEditor::refreshAll()
{
    lastVersion = processor.getSequenceVersion();

    const auto settings = processor.getSettings();
    paletteLabel.setText ("Chords in " + ChordEngine::getKeyNames()[settings.key] + " "
                              + ChordEngine::getScaleNames()[settings.scale]
                              + "  -  click to play, drag or double-click to add",
                          juce::dontSendNotification);

    palette.refresh();
    progression.refresh();
}

std::vector<ChordEngine::ChordRef> ChordPlayerEditor::refsFromDescription (const juce::String& description) const
{
    const auto parts = juce::StringArray::fromTokens (description, ":", "");

    if (parts[0] == "palette" && parts.size() == 3)
        return { { parts[1].getIntValue(), parts[2].getIntValue(), 0 } };

    if (parts[0] == "prog" && parts.size() == 2)
    {
        const auto chords = processor.getProgression();
        const int index = parts[1].getIntValue();

        if (juce::isPositiveAndBelow (index, (int) chords.size()))
            return { chords[(size_t) index] };
    }

    return {};
}

bool ChordPlayerEditor::shouldDropFilesWhenDraggedExternally (const juce::DragAndDropTarget::SourceDetails& details,
                                                              juce::StringArray& files, bool& canMoveFiles)
{
    const auto refs = refsFromDescription (details.description.toString());

    if (refs.empty())
        return false;

    auditioner.stop();
    files.add (writeMidiFile (refs).getFullPathName());
    canMoveFiles = false;
    return true;
}

juce::File ChordPlayerEditor::writeMidiFile (const std::vector<ChordEngine::ChordRef>& refs) const
{
    juce::StringArray names;

    for (const auto& ref : refs)
    {
        const auto chord = processor.getChord (ref.degree, ref.type);

        if (chord.valid)
            names.add (chord.name);
    }

    // Chord names only contain letters, digits, '#' and spaces, which are all valid in file names.
    auto fileName = names.joinIntoString (" ").substring (0, 80);
    if (fileName.isEmpty())
        fileName = "Chords";

    auto folder = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("Jazza Chords");
    folder.createDirectory();

    auto file = folder.getChildFile (fileName + ".mid");
    file.deleteFile();

    const auto midi = ChordEngine::createMidiFile (processor.renderSequence (refs), processor.getBpm());

    {
        juce::FileOutputStream stream (file);

        if (stream.openedOk())
            midi.writeTo (stream);
    }

    return file;
}
