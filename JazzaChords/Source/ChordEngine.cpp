#include "ChordEngine.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace ChordEngine
{
namespace
{
    const char* const keyNames[] = { "C", "Db", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B" };

    struct ScaleDef
    {
        const char* name;
        std::array<int, 7> steps;
    };

    const ScaleDef scales[] = {
        { "Major",          { 0, 2, 4, 5, 7, 9, 11 } },
        { "Minor",          { 0, 2, 3, 5, 7, 8, 10 } },
        { "Harmonic Minor", { 0, 2, 3, 5, 7, 8, 11 } },
        { "Melodic Minor",  { 0, 2, 3, 5, 7, 9, 11 } },
        { "Dorian",         { 0, 2, 3, 5, 7, 9, 10 } },
        { "Phrygian",       { 0, 1, 3, 5, 7, 8, 10 } },
        { "Lydian",         { 0, 2, 4, 6, 7, 9, 11 } },
        { "Mixolydian",     { 0, 2, 4, 5, 7, 9, 10 } },
    };

    enum Style { block = 0, quarters, eighths, dance, arpUp, arpDown, arpUpDown, broken, strum };

    const char* const styleNames[] = { "Block", "Quarters", "Eighths", "Dance", "Arp Up", "Arp Down", "Arp Up-Down", "Broken", "Strum" };
    const char* const voicingNames[] = { "Close", "Smooth", "Open" };
    const char* const lengthNames[] = { "1 beat", "2 beats", "1 bar", "2 bars" };
    const char* const chordTypeNames[] = { "sus2", "Triad", "sus4", "7th", "9th" };
    constexpr double lengthBeats[] = { 1.0, 2.0, 4.0, 8.0 };

    template <size_t N>
    juce::StringArray toStringArray (const char* const (&names)[N])
    {
        juce::StringArray result;
        for (auto* name : names)
            result.add (name);
        return result;
    }

    const ScaleDef& getScale (int index)
    {
        return scales[(size_t) juce::jlimit (0, (int) std::size (scales) - 1, index)];
    }

    // Semitone offset of a scale step, continuing upwards past the octave.
    int scaleNote (const ScaleDef& scale, int index)
    {
        return scale.steps[(size_t) (index % 7)] + 12 * (index / 7);
    }

    juce::String chordSuffix (const std::vector<int>& iv, int type)
    {
        if (type == sus2) return "sus2";
        if (type == sus4) return "sus4";

        const int third = iv[1], fifth = iv[2];
        const bool major = third == 4 && fifth == 7;
        const bool minor = third == 3 && fifth == 7;
        const bool diminished = third == 3 && fifth == 6;
        const bool augmented = third == 4 && fifth == 8;

        if (type == triad)
            return major ? "" : minor ? "m" : diminished ? "dim" : augmented ? "aug" : "?";

        const int seventhInterval = iv[3];
        juce::String name;

        if (major)           name = seventhInterval == 11 ? "maj7" : "7";
        else if (minor)      name = seventhInterval == 11 ? "mMaj7" : "m7";
        else if (diminished) name = seventhInterval == 9 ? "dim7" : "m7b5";
        else if (augmented)  name = seventhInterval == 11 ? "maj7#5" : "7#5";
        else                 name = "7";

        if (type == seventh)
            return name;

        switch (iv[4])
        {
            case 13: return name + "b9";
            case 15: return name + "#9";
            default: return name.replace ("7", "9");
        }
    }

    void addPattern (std::vector<NoteEvent>& events, const std::vector<int>& notes, int bass,
                     int style, double start, double length)
    {
        constexpr double gate = 0.98;

        auto addNote = [&] (double offset, double noteLength, int note, float velocity)
        {
            if (offset < length)
                events.push_back ({ start + offset, juce::jmin (noteLength, length - offset), note, velocity });
        };

        auto addChord = [&] (double offset, double noteLength, float velocity)
        {
            for (auto note : notes)
                addNote (offset, noteLength, note, velocity);
        };

        auto addBass = [&] (double offset, double noteLength, float velocity)
        {
            if (bass >= 0)
                addNote (offset, noteLength, bass, velocity);
        };

        auto accent = [] (double beat) { return std::fmod (beat, 1.0) == 0.0 ? 0.82f : 0.68f; };

        switch (style)
        {
            case quarters:
                for (double b = 0.0; b < length; b += 1.0)
                    addChord (b, 0.9, b == 0.0 ? 0.85f : 0.72f);
                addBass (0.0, length * gate, 0.85f);
                break;

            case eighths:
                for (double b = 0.0; b < length; b += 0.5)
                    addChord (b, 0.45, accent (b));
                for (double b = 0.0; b < length; b += 1.0)
                    addBass (b, 0.9, 0.8f);
                break;

            case dance:
                for (double b = 0.0; b < length; b += 1.0)
                {
                    addBass (b, 0.45, 0.9f);
                    addChord (b + 0.5, 0.4, 0.75f);
                }
                break;

            case arpUp:
            case arpDown:
            case arpUpDown:
            {
                std::vector<int> pattern (notes.begin(), notes.end());
                std::sort (pattern.begin(), pattern.end());
                pattern.push_back (pattern.front() + 12);

                if (style == arpDown)
                    std::reverse (pattern.begin(), pattern.end());
                else if (style == arpUpDown && pattern.size() > 2)
                {
                    const std::vector<int> descending (pattern.rbegin() + 1, pattern.rend() - 1);
                    pattern.insert (pattern.end(), descending.begin(), descending.end());
                }

                int step = 0;
                for (double b = 0.0; b < length; b += 0.5, ++step)
                    addNote (b, 0.45, pattern[(size_t) step % pattern.size()], step % 2 == 0 ? 0.8f : 0.66f);

                addBass (0.0, length * gate, 0.8f);
                break;
            }

            case broken:
            {
                std::vector<int> sorted (notes.begin(), notes.end());
                std::sort (sorted.begin(), sorted.end());
                const int pattern[] = { sorted.front(), sorted.back(), sorted[sorted.size() / 2], sorted.back() };

                int step = 0;
                for (double b = 0.0; b < length; b += 0.5, ++step)
                    addNote (b, 0.45, pattern[step % 4], step % 2 == 0 ? 0.78f : 0.64f);

                addBass (0.0, length * gate, 0.8f);
                break;
            }

            case strum:
            {
                std::vector<int> sorted (notes.begin(), notes.end());
                std::sort (sorted.begin(), sorted.end());

                for (size_t i = 0; i < sorted.size(); ++i)
                {
                    const double offset = 0.04 * (double) i;
                    addNote (offset, length * gate - offset, sorted[i], 0.8f - 0.03f * (float) i);
                }

                addBass (0.0, length * gate, 0.85f);
                break;
            }

            case block:
            default:
                addChord (0.0, length * gate, 0.8f);
                addBass (0.0, length * gate, 0.8f);
                break;
        }
    }
}

juce::StringArray getKeyNames()       { return toStringArray (keyNames); }
juce::StringArray getStyleNames()     { return toStringArray (styleNames); }
juce::StringArray getVoicingNames()   { return toStringArray (voicingNames); }
juce::StringArray getLengthNames()    { return toStringArray (lengthNames); }
juce::StringArray getChordTypeNames() { return toStringArray (chordTypeNames); }

juce::StringArray getScaleNames()
{
    juce::StringArray result;
    for (auto& scale : scales)
        result.add (scale.name);
    return result;
}

double getLengthBeats (int lengthIndex)
{
    return lengthBeats[(size_t) juce::jlimit (0, (int) std::size (lengthBeats) - 1, lengthIndex)];
}

Chord getChord (int key, int scaleIndex, int degree, int type)
{
    const auto& scale = getScale (scaleIndex);
    degree = juce::jlimit (0, numDegrees - 1, degree);

    std::vector<int> steps;
    switch (type)
    {
        case sus2:    steps = { 0, 1, 4 }; break;
        case sus4:    steps = { 0, 3, 4 }; break;
        case seventh: steps = { 0, 2, 4, 6 }; break;
        case ninth:   steps = { 0, 2, 4, 6, 8 }; break;
        default:      steps = { 0, 2, 4 }; type = triad; break;
    }

    Chord chord;
    const int rootOffset = scale.steps[(size_t) degree];
    chord.root = (key + rootOffset) % 12;

    for (auto step : steps)
        chord.intervals.push_back (scaleNote (scale, degree + step) - rootOffset);

    // Stacking scale steps doesn't always give a real sus chord, e.g. E-F-B in C major.
    if (type == sus2)
        chord.valid = chord.intervals == std::vector<int> { 0, 2, 7 };
    else if (type == sus4)
        chord.valid = chord.intervals == std::vector<int> { 0, 5, 7 };
    else
        chord.valid = true;

    chord.name = juce::String (keyNames[chord.root]) + chordSuffix (chord.intervals, type);
    return chord;
}

std::vector<int> voiceChord (const Chord& chord, int octave, int voicing, const std::vector<int>& previous)
{
    const int base = 12 * (octave + 1);

    std::vector<int> notes;
    for (auto interval : chord.intervals)
        notes.push_back (base + chord.root + interval);

    if (voicing == openVoicing && notes.size() >= 3)
    {
        // Drop 2: move the second-highest note down an octave.
        notes[notes.size() - 2] -= 12;
        std::sort (notes.begin(), notes.end());
    }
    else if (voicing == smoothVoicing && ! previous.empty())
    {
        // Pick the inversion and octave that moves the least from the previous chord.
        auto candidate = notes;
        auto best = notes;
        double bestScore = 1.0e9;

        for (size_t inversion = 0; inversion < notes.size(); ++inversion)
        {
            for (int shift : { -12, 0, 12 })
            {
                double score = 0.0, mean = 0.0;

                for (auto note : candidate)
                {
                    int nearest = 1000;
                    for (auto p : previous)
                        nearest = juce::jmin (nearest, std::abs (note + shift - p));

                    score += nearest;
                    mean += note + shift;
                }

                mean /= (double) candidate.size();
                score += 0.25 * std::abs (mean - (base + 7));

                if (score < bestScore)
                {
                    bestScore = score;
                    best = candidate;
                    for (auto& note : best)
                        note += shift;
                }
            }

            candidate.front() += 12;
            std::sort (candidate.begin(), candidate.end());
        }

        notes = best;
    }

    return notes;
}

int getBassNote (const Chord& chord, int octave)
{
    return juce::jmax (24, 12 * (octave + 1) - 24 + chord.root);
}

Sequence render (const std::vector<ChordRef>& progression, const Settings& settings)
{
    Sequence sequence;
    std::vector<int> previous;
    double time = 0.0;

    for (const auto& ref : progression)
    {
        const double length = ref.beats > 0 ? (double) ref.beats : settings.defaultBeats;
        sequence.chordStarts.push_back (time);

        const auto chord = getChord (settings.key, settings.scale, ref.degree, ref.type);

        if (chord.valid)
        {
            const auto notes = voiceChord (chord, settings.octave, settings.voicing, previous);
            const int bass = settings.bass ? getBassNote (chord, settings.octave) : -1;
            addPattern (sequence.events, notes, bass, settings.style, time, length);
            previous = notes;
        }

        time += length;
    }

    sequence.length = time;
    return sequence;
}

juce::MidiFile createMidiFile (const Sequence& sequence, double bpm)
{
    constexpr int ticksPerBeat = 960;

    juce::MidiMessageSequence track;
    track.addEvent (juce::MidiMessage::tempoMetaEvent (juce::roundToInt (60000000.0 / bpm)), 0.0);
    track.addEvent (juce::MidiMessage::timeSignatureMetaEvent (4, 4), 0.0);

    for (const auto& event : sequence.events)
    {
        track.addEvent (juce::MidiMessage::noteOn (1, event.note, event.velocity), event.start * ticksPerBeat);
        track.addEvent (juce::MidiMessage::noteOff (1, event.note), (event.start + event.length) * ticksPerBeat);
    }

    track.updateMatchedPairs();
    track.addEvent (juce::MidiMessage::endOfTrack(), sequence.length * ticksPerBeat);

    juce::MidiFile file;
    file.setTicksPerQuarterNote (ticksPerBeat);
    file.addTrack (track);
    return file;
}

std::vector<ChordRef> randomProgression (juce::Random& random, int key, int scale)
{
    static const int templates[][4] = {
        { 0, 4, 5, 3 }, { 0, 5, 3, 4 }, { 5, 3, 0, 4 }, { 0, 3, 0, 4 },
        { 1, 4, 0, 5 }, { 0, 3, 5, 4 }, { 3, 4, 2, 5 }, { 0, 2, 3, 4 },
        { 5, 4, 3, 4 }, { 0, 5, 1, 4 }, { 3, 0, 4, 5 }, { 0, 4, 3, 3 },
    };

    const auto& degrees = templates[random.nextInt ((int) std::size (templates))];
    std::vector<ChordRef> progression;

    for (auto degree : degrees)
    {
        const float r = random.nextFloat();
        int type = r < 0.6f ? triad : r < 0.85f ? seventh : r < 0.95f ? (random.nextBool() ? sus2 : sus4) : ninth;

        if (! getChord (key, scale, degree, type).valid)
            type = triad;

        progression.push_back ({ degree, type, 0 });
    }

    return progression;
}
}
