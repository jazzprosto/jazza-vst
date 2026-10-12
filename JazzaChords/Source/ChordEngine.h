#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include <vector>

// Music theory and pattern rendering. Pure functions, no audio-thread state.
namespace ChordEngine
{
    enum ChordType { sus2 = 0, triad, sus4, seventh, ninth, numChordTypes };
    enum Voicing { closeVoicing = 0, smoothVoicing, openVoicing };

    inline constexpr int numDegrees = 7;

    // A chord in the progression, stored relative to the current key and scale.
    struct ChordRef
    {
        int degree = 0;
        int type = triad;
        int beats = 0; // 0 = use the global chord length
    };

    struct Chord
    {
        bool valid = false;
        int root = 0;               // pitch class 0-11
        std::vector<int> intervals; // semitones above the root, starting with 0
        juce::String name;
    };

    struct Settings
    {
        int key = 0;
        int scale = 0;
        int style = 0;
        int voicing = smoothVoicing;
        int octave = 4;
        double defaultBeats = 4.0;
        bool bass = true;
    };

    struct NoteEvent
    {
        double start = 0.0;  // beats
        double length = 0.0; // beats
        int note = 60;
        float velocity = 0.8f;
    };

    struct Sequence
    {
        std::vector<NoteEvent> events;
        std::vector<double> chordStarts;
        double length = 0.0;
    };

    juce::StringArray getKeyNames();
    juce::StringArray getScaleNames();
    juce::StringArray getStyleNames();
    juce::StringArray getVoicingNames();
    juce::StringArray getLengthNames();
    juce::StringArray getChordTypeNames();
    double getLengthBeats (int lengthIndex);

    Chord getChord (int key, int scale, int degree, int type);
    std::vector<int> voiceChord (const Chord& chord, int octave, int voicing, const std::vector<int>& previous);
    int getBassNote (const Chord& chord, int octave);

    Sequence render (const std::vector<ChordRef>& progression, const Settings& settings);
    juce::MidiFile createMidiFile (const Sequence& sequence, double bpm);

    std::vector<ChordRef> randomProgression (juce::Random& random, int key, int scale);
}
