#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

#include <array>

namespace ParamIDs
{
    inline constexpr const char* distOn     = "distOn";
    inline constexpr const char* distType   = "distType";
    inline constexpr const char* distDrive  = "distDrive";
    inline constexpr const char* distTone   = "distTone";
    inline constexpr const char* distOutput = "distOutput";
    inline constexpr const char* distMix    = "distMix";

    inline constexpr const char* delayOn       = "delayOn";
    inline constexpr const char* delayTime     = "delayTime";
    inline constexpr const char* delayFeedback = "delayFeedback";
    inline constexpr const char* delayTone     = "delayTone";
    inline constexpr const char* delayMix      = "delayMix";
    inline constexpr const char* delayPingPong = "delayPingPong";

    inline constexpr const char* revOn    = "revOn";
    inline constexpr const char* revSize  = "revSize";
    inline constexpr const char* revDamp  = "revDamp";
    inline constexpr const char* revWidth = "revWidth";
    inline constexpr const char* revMix   = "revMix";
}

class TriFXAudioProcessor : public juce::AudioProcessor
{
public:
    TriFXAudioProcessor();
    ~TriFXAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    void reset() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 10.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    juce::AudioProcessorValueTreeState apvts;

private:
    void processDistortion (juce::dsp::AudioBlock<float>& block);
    void processDelay (juce::AudioBuffer<float>& buffer);
    void processReverb (juce::dsp::AudioBlock<float>& block);

    static constexpr size_t oversamplingOrder = 2; // 2^2 = 4x
    static constexpr double maxDelaySeconds = 2.0;

    double currentSampleRate = 44100.0;

    // Distortion
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampling;
    juce::dsp::Gain<float> driveGain, outputGain;
    juce::dsp::StateVariableTPTFilter<float> toneFilter;
    juce::dsp::DryWetMixer<float> distMixer { 512 };

    // Delay
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Lagrange3rd> delayLine { 192000 * 2 };
    juce::SmoothedValue<float> delayTimeSmoothed, feedbackSmoothed, delayMixSmoothed;
    std::array<float, 2> dampState {};

    // Reverb
    juce::dsp::Reverb reverb;

    std::atomic<float>* distOn = nullptr;
    std::atomic<float>* distType = nullptr;
    std::atomic<float>* distDrive = nullptr;
    std::atomic<float>* distTone = nullptr;
    std::atomic<float>* distOutput = nullptr;
    std::atomic<float>* distMix = nullptr;

    std::atomic<float>* delayOn = nullptr;
    std::atomic<float>* delayTime = nullptr;
    std::atomic<float>* delayFeedback = nullptr;
    std::atomic<float>* delayTone = nullptr;
    std::atomic<float>* delayMix = nullptr;
    std::atomic<float>* delayPingPong = nullptr;

    std::atomic<float>* revOn = nullptr;
    std::atomic<float>* revSize = nullptr;
    std::atomic<float>* revDamp = nullptr;
    std::atomic<float>* revWidth = nullptr;
    std::atomic<float>* revMix = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TriFXAudioProcessor)
};
