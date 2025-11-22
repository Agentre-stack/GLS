#pragma once

#include <JuceHeader.h>

namespace pit
{
class SimplePitchShifter
{
public:
    void prepare (double sampleRate, int maxChannels)
    {
        sr = sampleRate;
        windowSamples = juce::jmax (128.0f, static_cast<float> (sr * 0.04));
        minDelaySamples = windowSamples;
        bufferSize = static_cast<int> (std::ceil (windowSamples * 4.0f));
        if (bufferSize < 512)
            bufferSize = 512;

        writePos = 0;
        delayLines.assign (maxChannels, std::vector<float> (bufferSize, 0.0f));
        grainStates.assign (maxChannels, { GrainState {}, GrainState {} });
    }

    void reset()
    {
        writePos = 0;
        for (auto& line : delayLines)
            std::fill (line.begin(), line.end(), 0.0f);
        for (auto& statePair : grainStates)
        {
            statePair[0].phase = 0.0f;
            statePair[1].phase = 0.5f;
        }
    }

    void process (juce::AudioBuffer<float>& buffer, float ratio)
    {
        if (delayLines.empty())
            return;

        const auto numSamples  = buffer.getNumSamples();
        const auto numChannels = juce::jmin ((int) delayLines.size(), buffer.getNumChannels());
        ratio = juce::jlimit (0.5f, 2.0f, ratio);
        const float slope = 1.0f - ratio;
        const float phaseIncrement = 1.0f / windowSamples;

        for (int sample = 0; sample < numSamples; ++sample)
        {
            for (int ch = 0; ch < numChannels; ++ch)
                delayLines[(size_t) ch][writePos] = buffer.getSample (ch, sample);

            for (int ch = 0; ch < numChannels; ++ch)
            {
                float mixed = 0.0f;
                for (int voice = 0; voice < 2; ++voice)
                {
                    auto& grain = grainStates[(size_t) ch][(size_t) voice];
                    const float offset = voice == 0 ? 0.0f : windowSamples * 0.5f;
                    float delaySamples = minDelaySamples + offset + slope * grain.phase * windowSamples;
                    delaySamples = juce::jlimit (32.0f, (float) bufferSize - 4.0f, delaySamples);

                    float readIndex = static_cast<float> (writePos) - delaySamples;
                    while (readIndex < 0.0f)
                        readIndex += static_cast<float> (bufferSize);

                    const auto index0 = static_cast<int> (readIndex);
                    const auto index1 = (index0 + 1) % bufferSize;
                    const float frac = readIndex - static_cast<float> (index0);
                    const auto& delay = delayLines[(size_t) ch];
                    const float s0 = delay[(size_t) index0];
                    const float s1 = delay[(size_t) index1];
                    float voiceSample = s0 + (s1 - s0) * frac;
                    const float window = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * grain.phase);
                    mixed += voiceSample * window;

                    grain.phase += phaseIncrement;
                    if (grain.phase >= 1.0f)
                        grain.phase -= 1.0f;
                }

                buffer.setSample (ch, sample, mixed);
            }

            writePos = (writePos + 1) % bufferSize;
        }
    }

private:
    struct GrainState
    {
        float phase = 0.0f;
    };

    double sr = 44100.0;
    float windowSamples = 2048.0f;
    float minDelaySamples = 512.0f;
    int bufferSize = 4096;
    int writePos = 0;
    std::vector<std::vector<float>> delayLines;
    std::vector<std::array<GrainState, 2>> grainStates;
};
} // namespace pit

