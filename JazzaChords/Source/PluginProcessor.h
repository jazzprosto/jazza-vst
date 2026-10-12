#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "ChordEngine.h"

namespace ParamIDs
{
    inline constexpr const char* volume   = "volume";
    inline constexpr const char* sound    = "sound";
    inline constexpr const char* style    = "style";
    inline constexpr const char* key      = "key";
    inline constexpr const char* scale    = "scale";
    inline constexpr const char* length   = "length";
    inline constexpr const char* voicing  = "voicing";
    inline constexpr const char* octave   = "octave";
    inline constexpr const char* bass     = "bass";
    inline constexpr const char* hostSync = "hostSync";
}

class ChordPlayerProcessor : public juce::AudioProcessor,
                             private juce::AudioProcessorValueTreeState::Listener,
                             private juce::AsyncUpdater
{
public:
    ChordPlayerProcessor();
    ~ChordPlayerProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 1.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // Message-thread API used by the editor
    std::vector<ChordEngine::ChordRef> getProgression() const;
    void setProgression (const std::vector<ChordEngine::ChordRef>& progression);
    ChordEngine::Settings getSettings() const;
    ChordEngine::Chord getChord (int degree, int type) const;
    std::vector<int> getChordNotes (int degree, int type) const; // voiced notes + bass, for auditioning
    ChordEngine::Sequence renderSequence (const std::vector<ChordEngine::ChordRef>& progression) const;

    double getBpm() const { return bpm.load(); }
    bool isPlaying() const { return playing.load(); }
    void setInternalPlaying (bool shouldPlay) { internalPlaying = shouldPlay; }
    bool isInternalPlaying() const { return internalPlaying.load(); }
    int getCurrentChordIndex() const { return currentChord.load(); }
    int getSequenceVersion() const { return sequenceVersion.load(); }

    juce::AudioProcessorValueTreeState apvts;
    juce::MidiKeyboardState keyboardState;

private:
    void parameterChanged (const juce::String&, float) override { triggerAsyncUpdate(); }
    void handleAsyncUpdate() override { rebuildSequence(); }
    void rebuildSequence();

    void generateSequencerMidi (juce::MidiBuffer& midi, int numSamples);
    void allSequencerNotesOff (juce::MidiBuffer& midi, int samplePosition);

    struct ActiveNote { int note; double endBeat; };
    struct PendingEvent { int sample; bool isNoteOn; int note; float velocity; };

    juce::Synthesiser synth;

    juce::SpinLock sequenceLock;
    ChordEngine::Sequence sequence;

    std::vector<ActiveNote> activeNotes;
    std::vector<PendingEvent> pendingEvents;

    std::atomic<bool> internalPlaying { false }, playing { false };
    std::atomic<double> bpm { 120.0 };
    std::atomic<int> currentChord { -1 }, sequenceVersion { 0 };

    double currentSampleRate = 44100.0;
    double internalBeat = 0.0, expectedBeat = 0.0;
    bool wasPlaying = false;
    float lastVolume = 0.8f;

    std::atomic<float>* volumeParam = nullptr;
    std::atomic<float>* soundParam = nullptr;
    std::atomic<float>* hostSyncParam = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChordPlayerProcessor)
};
