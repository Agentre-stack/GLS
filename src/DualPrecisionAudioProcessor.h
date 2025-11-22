#pragma once

#include <JuceHeader.h>

class DualPrecisionAudioProcessor : public juce::AudioProcessor
{
public:
    explicit DualPrecisionAudioProcessor (const BusesProperties& ioConfig)
        : juce::AudioProcessor (ioConfig)
    {
    }

    bool supportsDoublePrecisionProcessing() const override { return true; }

    void processBlock (juce::AudioBuffer<double>& buffer, juce::MidiBuffer& midi) override
    {
        scratchBuffer.setSize (buffer.getNumChannels(), buffer.getNumSamples(), false, false, true);

        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            const auto* src = buffer.getReadPointer (ch);
            auto* dst = scratchBuffer.getWritePointer (ch);
            for (int i = 0; i < buffer.getNumSamples(); ++i)
                dst[i] = static_cast<float> (src[i]);
        }

        processBlock (scratchBuffer, midi);

        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            const auto* src = scratchBuffer.getReadPointer (ch);
            auto* dst = buffer.getWritePointer (ch);
            for (int i = 0; i < buffer.getNumSamples(); ++i)
                dst[i] = static_cast<double> (src[i]);
        }
    }

protected:
    using juce::AudioProcessor::AudioProcessor;
    using juce::AudioProcessor::processBlock;

private:
    juce::AudioBuffer<float> scratchBuffer;
};
