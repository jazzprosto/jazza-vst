#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include <array>
#include <atomic>
#include <cmath>

struct ChordSound : public juce::SynthesiserSound
{
    bool appliesToNote (int) override { return true; }
    bool appliesToChannel (int) override { return true; }
};

// A small additive / FM voice so the plugin makes sound on its own.
class ChordVoice : public juce::SynthesiserVoice
{
public:
    enum Sound { piano = 0, electricPiano, pad, organ };

    explicit ChordVoice (std::atomic<float>& soundParameter) : soundParam (soundParameter) {}

    bool canPlaySound (juce::SynthesiserSound* s) override { return dynamic_cast<ChordSound*> (s) != nullptr; }

    void startNote (int midiNote, float velocity, juce::SynthesiserSound*, int) override
    {
        sound = (int) soundParam.load();
        frequency = juce::MidiMessage::getMidiNoteInHertz (midiNote);
        level = 0.16f * velocity;
        phase = phase2 = 0.0;

        const double sampleRate = getSampleRate();

        // Higher piano harmonics die away faster, like a real string.
        for (size_t h = 0; h < harmonicEnv.size(); ++h)
        {
            harmonicEnv[h] = 1.0;
            harmonicDecay[h] = std::exp (-(0.5 + 0.45 * (double) (h + 1)) / sampleRate);
        }

        fmIndexEnv = 1.0;
        fmIndexDecay = std::exp (-2.5 / sampleRate);
        ampEnv = 1.0;
        ampDecay = std::exp (-0.9 / sampleRate);

        juce::ADSR::Parameters params;
        switch (sound)
        {
            case pad:           params = { 0.35f, 0.6f, 0.8f, 0.9f }; break;
            case organ:         params = { 0.005f, 0.05f, 1.0f, 0.06f }; break;
            case electricPiano: params = { 0.002f, 0.05f, 1.0f, 0.35f }; break;
            default:            params = { 0.002f, 0.05f, 1.0f, 0.3f }; break;
        }

        adsr.setSampleRate (sampleRate);
        adsr.setParameters (params);
        adsr.noteOn();
    }

    void stopNote (float, bool allowTailOff) override
    {
        if (allowTailOff)
        {
            adsr.noteOff();
        }
        else
        {
            adsr.reset();
            clearCurrentNote();
        }
    }

    void pitchWheelMoved (int) override {}
    void controllerMoved (int, int) override {}

    void renderNextBlock (juce::AudioBuffer<float>& output, int startSample, int numSamples) override
    {
        if (! isVoiceActive())
            return;

        const double sampleRate = getSampleRate();
        const double increment = frequency / sampleRate;
        const double increment2 = frequency * 1.006 / sampleRate;
        const int maxHarmonic = juce::jmax (1, (int) (sampleRate * 0.45 / frequency));

        for (int i = 0; i < numSamples; ++i)
        {
            const float sample = (float) computeSample (maxHarmonic) * adsr.getNextSample() * level;

            for (int ch = 0; ch < output.getNumChannels(); ++ch)
                output.addSample (ch, startSample + i, sample);

            phase += increment;
            if (phase >= 1.0) phase -= 1.0;

            phase2 += increment2;
            if (phase2 >= 1.0) phase2 -= 1.0;

            for (size_t h = 0; h < harmonicEnv.size(); ++h)
                harmonicEnv[h] *= harmonicDecay[h];

            fmIndexEnv *= fmIndexDecay;
            ampEnv *= ampDecay;

            if (! adsr.isActive())
            {
                clearCurrentNote();
                break;
            }
        }
    }

private:
    double computeSample (int maxHarmonic) const
    {
        constexpr double twoPi = juce::MathConstants<double>::twoPi;

        switch (sound)
        {
            case electricPiano:
            {
                const double index = 1.8 * fmIndexEnv + 0.15;
                return std::sin (twoPi * phase + index * std::sin (twoPi * phase)) * ampEnv * 1.2;
            }

            case pad:
            {
                double s = 0.0;
                for (int h = 1; h <= juce::jmin (8, maxHarmonic); ++h)
                    s += (std::sin (twoPi * h * phase) + std::sin (twoPi * h * phase2)) / h;
                return s * 0.35;
            }

            case organ:
            {
                static constexpr int harmonics[] = { 1, 2, 3, 4, 6, 8 };
                static constexpr double amps[] = { 1.0, 0.7, 0.5, 0.35, 0.25, 0.2 };

                double s = 0.0;
                for (size_t k = 0; k < std::size (harmonics); ++k)
                    if (harmonics[k] <= maxHarmonic)
                        s += amps[k] * std::sin (twoPi * harmonics[k] * phase);
                return s * 0.4;
            }

            case piano:
            default:
            {
                static constexpr double amps[] = { 1.0, 0.55, 0.32, 0.22, 0.14, 0.09, 0.06, 0.04 };

                double s = 0.0;
                for (int h = 1; h <= 8 && h <= maxHarmonic; ++h)
                    s += amps[h - 1] * std::sin (twoPi * h * phase) * harmonicEnv[(size_t) h - 1];
                return s * 0.55;
            }
        }
    }

    std::atomic<float>& soundParam;
    juce::ADSR adsr;

    int sound = piano;
    double frequency = 440.0, phase = 0.0, phase2 = 0.0;
    float level = 0.0f;

    std::array<double, 8> harmonicEnv {}, harmonicDecay {};
    double fmIndexEnv = 1.0, fmIndexDecay = 1.0, ampEnv = 1.0, ampDecay = 1.0;
};
