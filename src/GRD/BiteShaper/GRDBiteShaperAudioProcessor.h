#pragma once

#include <JuceHeader.h>
#include "../../DualPrecisionAudioProcessor.h"

class GRDBiteShaperAudioProcessor : public DualPrecisionAudioProcessor
{
public:
    GRDBiteShaperAudioProcessor();
    ~GRDBiteShaperAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "GRDBiteShaper"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int index) override { return index == 0 ? juce::String (JucePlugin_Name " 01") : juce::String(); }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getValueTreeState() { return apvts; }
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

private:
    struct ChannelState
    {
        juce::dsp::IIR::Filter<float> toneFilter;
    };

    juce::AudioProcessorValueTreeState apvts;
    std::vector<ChannelState> channelState;
    double currentSampleRate = 44100.0;
    juce::uint32 lastBlockSize = 512;
    double filterSpecSampleRate = 0.0;
    juce::uint32 filterSpecBlockSize = 0;

    void ensureStateSize (int numChannels);
    void updateToneFilters (float toneHz);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GRDBiteShaperAudioProcessor)
};

class GRDBiteShaperAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit GRDBiteShaperAudioProcessorEditor (GRDBiteShaperAudioProcessor&);
    ~GRDBiteShaperAudioProcessorEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    GRDBiteShaperAudioProcessor& processorRef;

    juce::Slider biteSlider;
    juce::Slider foldSlider;
    juce::Slider toneSlider;
    juce::Slider mixSlider;
    juce::Slider trimSlider;

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::vector<std::unique_ptr<SliderAttachment>> attachments;

    void initSlider (juce::Slider&, const juce::String&);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GRDBiteShaperAudioProcessorEditor)
};
