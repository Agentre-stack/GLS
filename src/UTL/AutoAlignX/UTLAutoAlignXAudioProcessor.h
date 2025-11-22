#pragma once

#include <JuceHeader.h>
#include "../../DualPrecisionAudioProcessor.h"

class UTLAutoAlignXAudioProcessor : public DualPrecisionAudioProcessor
{
public:
    UTLAutoAlignXAudioProcessor();
    ~UTLAutoAlignXAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "UTLAutoAlignX"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int index) override { return index == 0 ? juce::String (JucePlugin_Name \" 01\") : juce::String(); }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getValueTreeState() { return apvts; }
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

private:
    struct ChannelDelay
    {
        juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> delay { 48000 };
    };

    juce::AudioProcessorValueTreeState apvts;
    std::vector<ChannelDelay> channelDelays;
    double currentSampleRate = 44100.0;
    juce::uint32 lastBlockSize = 512;
    double delaySpecSampleRate = 0.0;
    juce::uint32 delaySpecBlockSize = 0;

    void ensureDelayState (int numChannels);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (UTLAutoAlignXAudioProcessor)
};

class UTLAutoAlignXAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit UTLAutoAlignXAudioProcessorEditor (UTLAutoAlignXAudioProcessor&);
    ~UTLAutoAlignXAudioProcessorEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    UTLAutoAlignXAudioProcessor& processorRef;

    juce::Slider delayLeftSlider;
    juce::Slider delayRightSlider;
    juce::ToggleButton invertLeftButton;
    juce::ToggleButton invertRightButton;

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    std::vector<std::unique_ptr<SliderAttachment>> sliderAttachments;
    std::vector<std::unique_ptr<ButtonAttachment>> buttonAttachments;

    void initSlider (juce::Slider&, const juce::String&);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (UTLAutoAlignXAudioProcessorEditor)
};
