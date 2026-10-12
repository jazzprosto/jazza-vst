#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "SynthVoice.h"

#include <algorithm>
#include <cmath>

namespace
{
    const juce::Identifier progressionId { "PROGRESSION" };
    const juce::Identifier chordId { "CHORD" };
    const juce::Identifier degreeId { "degree" };
    const juce::Identifier typeId { "type" };
    const juce::Identifier beatsId { "beats" };

    constexpr int numVoices = 24;

    // Same opening progression as the web app: C - Em7 - Am - F
    const std::vector<ChordEngine::ChordRef> defaultProgression {
        { 0, ChordEngine::triad, 0 },
        { 2, ChordEngine::seventh, 0 },
        { 5, ChordEngine::triad, 0 },
        { 3, ChordEngine::triad, 0 },
    };

    const char* const sequenceParams[] = { ParamIDs::style, ParamIDs::key, ParamIDs::scale, ParamIDs::length,
                                           ParamIDs::voicing, ParamIDs::octave, ParamIDs::bass };
}

ChordPlayerProcessor::ChordPlayerProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Parameters", createParameterLayout())
{
    volumeParam = apvts.getRawParameterValue (ParamIDs::volume);
    soundParam = apvts.getRawParameterValue (ParamIDs::sound);
    hostSyncParam = apvts.getRawParameterValue (ParamIDs::hostSync);

    for (int i = 0; i < numVoices; ++i)
        synth.addVoice (new ChordVoice (*soundParam));

    synth.addSound (new ChordSound());

    for (auto* id : sequenceParams)
        apvts.addParameterListener (id, this);

    setProgression (defaultProgression);
}

ChordPlayerProcessor::~ChordPlayerProcessor()
{
    for (auto* id : sequenceParams)
        apvts.removeParameterListener (id, this);

    cancelPendingUpdate();
}

juce::AudioProcessorValueTreeState::ParameterLayout ChordPlayerProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    auto addChoice = [&layout] (const char* id, const char* name, const juce::StringArray& choices, int defaultIndex)
    {
        layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { id, 1 }, name, choices, defaultIndex));
    };

    auto addBool = [&layout] (const char* id, const char* name, bool defaultValue)
    {
        layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { id, 1 }, name, defaultValue));
    };

    layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { ParamIDs::volume, 1 }, "Volume",
                                                             juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.8f));

    addChoice (ParamIDs::sound,   "Sound",   { "Piano", "E-Piano", "Pad", "Organ" }, 0);
    addChoice (ParamIDs::style,   "Style",   ChordEngine::getStyleNames(), 0);
    addChoice (ParamIDs::key,     "Key",     ChordEngine::getKeyNames(), 0);
    addChoice (ParamIDs::scale,   "Scale",   ChordEngine::getScaleNames(), 0);
    addChoice (ParamIDs::length,  "Length",  ChordEngine::getLengthNames(), 2);
    addChoice (ParamIDs::voicing, "Voicing", ChordEngine::getVoicingNames(), ChordEngine::smoothVoicing);
    addChoice (ParamIDs::octave,  "Octave",  { "2", "3", "4", "5", "6" }, 2);
    addBool (ParamIDs::bass, "Bass", true);
    addBool (ParamIDs::hostSync, "Sync to Host", true);

    return layout;
}

bool ChordPlayerProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& output = layouts.getMainOutputChannelSet();
    return output == juce::AudioChannelSet::mono() || output == juce::AudioChannelSet::stereo();
}

//==============================================================================
std::vector<ChordEngine::ChordRef> ChordPlayerProcessor::getProgression() const
{
    std::vector<ChordEngine::ChordRef> progression;

    for (const auto& child : apvts.state.getChildWithName (progressionId))
        progression.push_back ({ (int) child.getProperty (degreeId), (int) child.getProperty (typeId),
                                 (int) child.getProperty (beatsId) });

    return progression;
}

void ChordPlayerProcessor::setProgression (const std::vector<ChordEngine::ChordRef>& progression)
{
    auto tree = apvts.state.getOrCreateChildWithName (progressionId, nullptr);
    tree.removeAllChildren (nullptr);

    for (const auto& ref : progression)
    {
        juce::ValueTree chord (chordId);
        chord.setProperty (degreeId, ref.degree, nullptr);
        chord.setProperty (typeId, ref.type, nullptr);
        chord.setProperty (beatsId, ref.beats, nullptr);
        tree.appendChild (chord, nullptr);
    }

    rebuildSequence();
}

ChordEngine::Settings ChordPlayerProcessor::getSettings() const
{
    auto choice = [this] (const char* id) { return (int) apvts.getRawParameterValue (id)->load(); };

    ChordEngine::Settings settings;
    settings.key = choice (ParamIDs::key);
    settings.scale = choice (ParamIDs::scale);
    settings.style = choice (ParamIDs::style);
    settings.voicing = choice (ParamIDs::voicing);
    settings.octave = choice (ParamIDs::octave) + 2;
    settings.defaultBeats = ChordEngine::getLengthBeats (choice (ParamIDs::length));
    settings.bass = apvts.getRawParameterValue (ParamIDs::bass)->load() > 0.5f;
    return settings;
}

ChordEngine::Chord ChordPlayerProcessor::getChord (int degree, int type) const
{
    const auto settings = getSettings();
    return ChordEngine::getChord (settings.key, settings.scale, degree, type);
}

std::vector<int> ChordPlayerProcessor::getChordNotes (int degree, int type) const
{
    const auto settings = getSettings();
    const auto chord = ChordEngine::getChord (settings.key, settings.scale, degree, type);

    if (! chord.valid)
        return {};

    auto notes = ChordEngine::voiceChord (chord, settings.octave, settings.voicing, {});

    if (settings.bass)
        notes.push_back (ChordEngine::getBassNote (chord, settings.octave));

    return notes;
}

ChordEngine::Sequence ChordPlayerProcessor::renderSequence (const std::vector<ChordEngine::ChordRef>& progression) const
{
    return ChordEngine::render (progression, getSettings());
}

void ChordPlayerProcessor::rebuildSequence()
{
    auto newSequence = renderSequence (getProgression());

    {
        const juce::SpinLock::ScopedLockType lock (sequenceLock);
        std::swap (sequence, newSequence);
    }

    ++sequenceVersion;
}

//==============================================================================
void ChordPlayerProcessor::prepareToPlay (double sampleRate, int)
{
    currentSampleRate = sampleRate;
    synth.setCurrentPlaybackSampleRate (sampleRate);
    keyboardState.reset();

    activeNotes.clear();
    activeNotes.reserve (512);
    pendingEvents.clear();
    pendingEvents.reserve (1024);

    wasPlaying = false;
    lastVolume = volumeParam->load();
}

void ChordPlayerProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    buffer.clear();

    generateSequencerMidi (midi, numSamples);

    // Adds notes clicked in the editor and lights up the on-screen keyboard.
    keyboardState.processNextMidiBuffer (midi, 0, numSamples, true);

    synth.renderNextBlock (buffer, midi, 0, numSamples);

    const float volume = volumeParam->load();
    buffer.applyGainRamp (0, numSamples, lastVolume, volume);
    lastVolume = volume;
}

void ChordPlayerProcessor::generateSequencerMidi (juce::MidiBuffer& midi, int numSamples)
{
    double hostBpm = 120.0, hostBeat = 0.0;
    bool hostPlaying = false;

    if (auto* hostPlayHead = getPlayHead())
    {
        if (auto position = hostPlayHead->getPosition())
        {
            if (auto b = position->getBpm())
                hostBpm = *b;

            if (auto ppq = position->getPpqPosition())
                hostBeat = *ppq;

            hostPlaying = position->getIsPlaying();
        }
    }

    bpm = hostBpm;

    const bool followHost = hostPlaying && hostSyncParam->load() > 0.5f;
    const bool nowPlaying = followHost || internalPlaying.load();

    if (! nowPlaying)
    {
        if (wasPlaying)
            allSequencerNotesOff (midi, 0);

        wasPlaying = false;
        playing = false;
        currentChord = -1;
        return;
    }

    const double beatsPerSample = hostBpm / (60.0 * currentSampleRate);
    double startBeat;

    if (followHost)
    {
        startBeat = hostBeat;
    }
    else
    {
        if (! wasPlaying)
            internalBeat = 0.0;

        startBeat = internalBeat;
        internalBeat += numSamples * beatsPerSample;
    }

    // The host jumped or looped: release whatever is still sounding.
    if (wasPlaying && std::abs (startBeat - expectedBeat) > 1.0e-3)
        allSequencerNotesOff (midi, 0);

    wasPlaying = true;
    playing = true;

    const double endBeat = startBeat + numSamples * beatsPerSample;
    expectedBeat = endBeat;

    auto sampleFor = [&] (double beat)
    {
        return juce::jlimit (0, numSamples - 1, (int) ((beat - startBeat) / beatsPerSample));
    };

    pendingEvents.clear();

    {
        const juce::SpinLock::ScopedTryLockType lock (sequenceLock);

        if (lock.isLocked() && sequence.length > 0.0)
        {
            const double loopLength = sequence.length;
            const double loopStart = std::floor (startBeat / loopLength) * loopLength;

            for (double offset = loopStart; offset < endBeat; offset += loopLength)
            {
                for (const auto& event : sequence.events)
                {
                    const double beat = offset + event.start;

                    if (beat >= startBeat && beat < endBeat
                        && pendingEvents.size() < pendingEvents.capacity()
                        && activeNotes.size() < activeNotes.capacity())
                    {
                        pendingEvents.push_back ({ sampleFor (beat), true, event.note, event.velocity });
                        activeNotes.push_back ({ event.note, beat + event.length });
                    }
                }
            }

            const double position = startBeat - loopStart;
            int index = -1;

            for (size_t i = 0; i < sequence.chordStarts.size(); ++i)
                if (position >= sequence.chordStarts[i])
                    index = (int) i;

            currentChord = index;
        }
        else if (lock.isLocked())
        {
            currentChord = -1;
        }
    }

    for (auto it = activeNotes.begin(); it != activeNotes.end();)
    {
        if (it->endBeat < endBeat && pendingEvents.size() < pendingEvents.capacity())
        {
            pendingEvents.push_back ({ sampleFor (it->endBeat), false, it->note, 0.0f });
            it = activeNotes.erase (it);
        }
        else
        {
            ++it;
        }
    }

    // Note-offs go before note-ons on the same sample so repeated notes retrigger cleanly.
    std::sort (pendingEvents.begin(), pendingEvents.end(), [] (const PendingEvent& a, const PendingEvent& b)
    {
        return a.sample != b.sample ? a.sample < b.sample : (! a.isNoteOn && b.isNoteOn);
    });

    for (const auto& e : pendingEvents)
        midi.addEvent (e.isNoteOn ? juce::MidiMessage::noteOn (1, e.note, e.velocity)
                                  : juce::MidiMessage::noteOff (1, e.note),
                       e.sample);
}

void ChordPlayerProcessor::allSequencerNotesOff (juce::MidiBuffer& midi, int samplePosition)
{
    for (const auto& note : activeNotes)
        midi.addEvent (juce::MidiMessage::noteOff (1, note.note), samplePosition);

    activeNotes.clear();
}

//==============================================================================
juce::AudioProcessorEditor* ChordPlayerProcessor::createEditor()
{
    return new ChordPlayerEditor (*this);
}

void ChordPlayerProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void ChordPlayerProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        if (xml->hasTagName (apvts.state.getType()))
        {
            apvts.replaceState (juce::ValueTree::fromXml (*xml));

            if (! apvts.state.getChildWithName (progressionId).isValid())
                setProgression (defaultProgression);
            else
                rebuildSequence();
        }
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ChordPlayerProcessor();
}
