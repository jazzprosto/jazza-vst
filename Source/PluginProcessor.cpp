#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <cmath>

namespace
{
    juce::String formatDb (float v, int)      { return juce::String (v, 1) + " dB"; }
    juce::String formatMs (float v, int)      { return juce::String (juce::roundToInt (v)) + " ms"; }
    juce::String formatPercent (float v, int) { return juce::String (juce::roundToInt (v * 100.0f)) + " %"; }
    float parsePercent (const juce::String& text) { return text.getFloatValue() / 100.0f; }

    juce::String formatHz (float v, int)
    {
        return v >= 1000.0f ? juce::String (v / 1000.0f, 1) + " kHz"
                            : juce::String (juce::roundToInt (v)) + " Hz";
    }

    template <typename Shaper>
    void applyShaper (juce::dsp::AudioBlock<float>& block, Shaper shaper)
    {
        for (size_t ch = 0; ch < block.getNumChannels(); ++ch)
        {
            auto* data = block.getChannelPointer (ch);

            for (size_t i = 0; i < block.getNumSamples(); ++i)
                data[i] = shaper (data[i]);
        }
    }
}

TriFXAudioProcessor::TriFXAudioProcessor()
    : AudioProcessor (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Parameters", createParameterLayout())
{
    distOn     = apvts.getRawParameterValue (ParamIDs::distOn);
    distType   = apvts.getRawParameterValue (ParamIDs::distType);
    distDrive  = apvts.getRawParameterValue (ParamIDs::distDrive);
    distTone   = apvts.getRawParameterValue (ParamIDs::distTone);
    distOutput = apvts.getRawParameterValue (ParamIDs::distOutput);
    distMix    = apvts.getRawParameterValue (ParamIDs::distMix);

    delayOn       = apvts.getRawParameterValue (ParamIDs::delayOn);
    delayTime     = apvts.getRawParameterValue (ParamIDs::delayTime);
    delayFeedback = apvts.getRawParameterValue (ParamIDs::delayFeedback);
    delayTone     = apvts.getRawParameterValue (ParamIDs::delayTone);
    delayMix      = apvts.getRawParameterValue (ParamIDs::delayMix);
    delayPingPong = apvts.getRawParameterValue (ParamIDs::delayPingPong);

    revOn    = apvts.getRawParameterValue (ParamIDs::revOn);
    revSize  = apvts.getRawParameterValue (ParamIDs::revSize);
    revDamp  = apvts.getRawParameterValue (ParamIDs::revDamp);
    revWidth = apvts.getRawParameterValue (ParamIDs::revWidth);
    revMix   = apvts.getRawParameterValue (ParamIDs::revMix);
}

juce::AudioProcessorValueTreeState::ParameterLayout TriFXAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    auto addFloat = [&layout] (const char* id, const char* name, juce::NormalisableRange<float> range, float defaultValue,
                               juce::AudioParameterFloatAttributes attributes)
    {
        layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { id, 1 }, name, range, defaultValue, attributes));
    };

    auto addBool = [&layout] (const char* id, const char* name, bool defaultValue)
    {
        layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { id, 1 }, name, defaultValue));
    };

    const auto db      = juce::AudioParameterFloatAttributes().withStringFromValueFunction (formatDb);
    const auto ms      = juce::AudioParameterFloatAttributes().withStringFromValueFunction (formatMs);
    const auto hz      = juce::AudioParameterFloatAttributes().withStringFromValueFunction (formatHz);
    const auto percent = juce::AudioParameterFloatAttributes().withStringFromValueFunction (formatPercent)
                                                              .withValueFromStringFunction (parsePercent);

    // Distortion
    addBool (ParamIDs::distOn, "Distortion On", true);
    layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { ParamIDs::distType, 1 }, "Distortion Type",
                                                              juce::StringArray { "Soft", "Hard", "Fold" }, 0));
    addFloat (ParamIDs::distDrive,  "Drive",           { 0.0f, 40.0f, 0.1f },              12.0f,   db);
    addFloat (ParamIDs::distTone,   "Distortion Tone", { 500.0f, 20000.0f, 1.0f, 0.3f },   8000.0f, hz);
    addFloat (ParamIDs::distOutput, "Distortion Out",  { -24.0f, 12.0f, 0.1f },            -6.0f,   db);
    addFloat (ParamIDs::distMix,    "Distortion Mix",  { 0.0f, 1.0f, 0.01f },              1.0f,    percent);

    // Delay
    addBool (ParamIDs::delayOn, "Delay On", true);
    addFloat (ParamIDs::delayTime,     "Delay Time",     { 1.0f, (float) (maxDelaySeconds * 1000.0), 1.0f, 0.5f }, 350.0f, ms);
    addFloat (ParamIDs::delayFeedback, "Delay Feedback", { 0.0f, 0.95f, 0.01f },             0.4f,    percent);
    addFloat (ParamIDs::delayTone,     "Delay Tone",     { 500.0f, 20000.0f, 1.0f, 0.3f },   6000.0f, hz);
    addFloat (ParamIDs::delayMix,      "Delay Mix",      { 0.0f, 1.0f, 0.01f },              0.3f,    percent);
    addBool (ParamIDs::delayPingPong, "Ping-Pong", false);

    // Reverb
    addBool (ParamIDs::revOn, "Reverb On", true);
    addFloat (ParamIDs::revSize,  "Reverb Size",    { 0.0f, 1.0f, 0.01f }, 0.5f,  percent);
    addFloat (ParamIDs::revDamp,  "Reverb Damping", { 0.0f, 1.0f, 0.01f }, 0.5f,  percent);
    addFloat (ParamIDs::revWidth, "Reverb Width",   { 0.0f, 1.0f, 0.01f }, 1.0f,  percent);
    addFloat (ParamIDs::revMix,   "Reverb Mix",     { 0.0f, 1.0f, 0.01f }, 0.25f, percent);

    return layout;
}

bool TriFXAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& output = layouts.getMainOutputChannelSet();

    if (output != juce::AudioChannelSet::mono() && output != juce::AudioChannelSet::stereo())
        return false;

    return output == layouts.getMainInputChannelSet();
}

void TriFXAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;

    const auto numChannels = (juce::uint32) juce::jmax (getTotalNumInputChannels(), getTotalNumOutputChannels());
    const juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) samplesPerBlock, numChannels };

    // Distortion: the waveshaper runs at 4x the host rate to keep aliasing down.
    oversampling = std::make_unique<juce::dsp::Oversampling<float>> (numChannels, oversamplingOrder,
                                                                     juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple,
                                                                     true, true);
    oversampling->initProcessing ((size_t) samplesPerBlock);

    const auto factor = oversampling->getOversamplingFactor();
    const juce::dsp::ProcessSpec oversampledSpec { sampleRate * (double) factor,
                                                   (juce::uint32) ((size_t) samplesPerBlock * factor),
                                                   numChannels };

    driveGain.prepare (oversampledSpec);
    driveGain.setRampDurationSeconds (0.05);

    toneFilter.prepare (spec);
    toneFilter.setType (juce::dsp::StateVariableTPTFilterType::lowpass);

    outputGain.prepare (spec);
    outputGain.setRampDurationSeconds (0.05);

    distMixer.prepare (spec);
    distMixer.setMixingRule (juce::dsp::DryWetMixingRule::linear);
    distMixer.setWetLatency (oversampling->getLatencyInSamples());

    // The oversampling filters add a few samples of latency; report it so hosts can compensate.
    setLatencySamples (juce::roundToInt (oversampling->getLatencyInSamples()));

    // Delay
    delayLine.prepare (spec);
    delayLine.setMaximumDelayInSamples ((int) std::ceil (sampleRate * maxDelaySeconds) + 4);

    delayTimeSmoothed.reset (sampleRate, 0.25);
    feedbackSmoothed.reset (sampleRate, 0.05);
    delayMixSmoothed.reset (sampleRate, 0.05);
    delayTimeSmoothed.setCurrentAndTargetValue (delayTime->load() * 0.001f * (float) sampleRate);
    feedbackSmoothed.setCurrentAndTargetValue (delayFeedback->load());
    delayMixSmoothed.setCurrentAndTargetValue (delayOn->load() > 0.5f ? delayMix->load() : 0.0f);

    // Reverb
    reverb.prepare (spec);

    reset();
}

void TriFXAudioProcessor::reset()
{
    if (oversampling != nullptr)
        oversampling->reset();

    driveGain.reset();
    outputGain.reset();
    toneFilter.reset();
    distMixer.reset();
    delayLine.reset();
    dampState.fill (0.0f);
    reverb.reset();
}

void TriFXAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    for (auto ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    juce::dsp::AudioBlock<float> block (buffer);

    // Signal chain: Distortion -> Delay -> Reverb
    processDistortion (block);
    processDelay (buffer);
    processReverb (block);
}

void TriFXAudioProcessor::processDistortion (juce::dsp::AudioBlock<float>& block)
{
    // Bypass is done by fading the wet signal out rather than skipping processing,
    // so latency stays constant and toggling doesn't click.
    const bool enabled = distOn->load() > 0.5f;
    distMixer.setWetMixProportion (enabled ? distMix->load() : 0.0f);

    driveGain.setGainDecibels (distDrive->load());
    outputGain.setGainDecibels (distOutput->load());
    toneFilter.setCutoffFrequency (juce::jmin (distTone->load(), (float) (currentSampleRate * 0.45)));

    distMixer.pushDrySamples (block);

    auto upsampled = oversampling->processSamplesUp (block);
    driveGain.process (juce::dsp::ProcessContextReplacing<float> (upsampled));

    switch ((int) distType->load())
    {
        case 1:  applyShaper (upsampled, [] (float x) { return juce::jlimit (-1.0f, 1.0f, x); }); break;
        case 2:  applyShaper (upsampled, [] (float x) { return std::sin (x); }); break;
        default: applyShaper (upsampled, [] (float x) { return std::tanh (x); }); break;
    }

    oversampling->processSamplesDown (block);

    juce::dsp::ProcessContextReplacing<float> context (block);
    toneFilter.process (context);
    outputGain.process (context);

    distMixer.mixWetSamples (block);
}

void TriFXAudioProcessor::processDelay (juce::AudioBuffer<float>& buffer)
{
    const int numChannels = juce::jmin (buffer.getNumChannels(), 2);
    const int numSamples = buffer.getNumSamples();
    const bool pingPong = delayPingPong->load() > 0.5f && numChannels == 2;

    delayTimeSmoothed.setTargetValue (delayTime->load() * 0.001f * (float) currentSampleRate);
    feedbackSmoothed.setTargetValue (delayFeedback->load());
    delayMixSmoothed.setTargetValue (delayOn->load() > 0.5f ? delayMix->load() : 0.0f);

    // One-pole lowpass in the feedback path, so each repeat gets a little darker.
    const float damp = std::exp (-juce::MathConstants<float>::twoPi * delayTone->load() / (float) currentSampleRate);

    for (int i = 0; i < numSamples; ++i)
    {
        const float delaySamples = delayTimeSmoothed.getNextValue();
        const float feedback = feedbackSmoothed.getNextValue();
        const float mix = delayMixSmoothed.getNextValue();

        float dry[2] {}, wet[2] {};

        for (int ch = 0; ch < numChannels; ++ch)
        {
            dry[ch] = buffer.getSample (ch, i);
            wet[ch] = delayLine.popSample (ch, delaySamples);
            dampState[(size_t) ch] = wet[ch] + damp * (dampState[(size_t) ch] - wet[ch]);
        }

        if (pingPong)
        {
            // Mono input enters the left side, then repeats bounce left <-> right.
            delayLine.pushSample (0, 0.5f * (dry[0] + dry[1]) + dampState[1] * feedback);
            delayLine.pushSample (1, dampState[0] * feedback);
        }
        else
        {
            for (int ch = 0; ch < numChannels; ++ch)
                delayLine.pushSample (ch, dry[ch] + dampState[(size_t) ch] * feedback);
        }

        for (int ch = 0; ch < numChannels; ++ch)
            buffer.setSample (ch, i, dry[ch] * (1.0f - mix) + wet[ch] * mix);
    }
}

void TriFXAudioProcessor::processReverb (juce::dsp::AudioBlock<float>& block)
{
    const bool enabled = revOn->load() > 0.5f;
    const float mix = enabled ? revMix->load() : 0.0f;

    // juce::Reverb scales dry by 2x and wet by 3x internally, so these levels give unity gain.
    juce::dsp::Reverb::Parameters params;
    params.roomSize = revSize->load();
    params.damping = revDamp->load();
    params.width = revWidth->load();
    params.wetLevel = mix / 3.0f;
    params.dryLevel = (1.0f - mix) * 0.5f;
    params.freezeMode = 0.0f;

    reverb.setParameters (params);
    reverb.process (juce::dsp::ProcessContextReplacing<float> (block));
}

juce::AudioProcessorEditor* TriFXAudioProcessor::createEditor()
{
    return new TriFXAudioProcessorEditor (*this);
}

void TriFXAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void TriFXAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new TriFXAudioProcessor();
}
