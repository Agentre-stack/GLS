#pragma once

#include <JuceHeader.h>

class DYNRMSRiderAudioProcessor : public juce::AudioProcessor
{
public:
    DYNRMSRiderAudioProcessor();
    ~DYNRMSRiderAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "DYNRMSRider"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getValueTreeState() { return apvts; }
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

private:
    juce::AudioProcessorValueTreeState apvts;
    struct ChannelState
    {
        juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> lookaheadLine { 48000 };
        float envelope = 0.0f;
    };

    std::vector<ChannelState> channelStates;
    double currentSampleRate = 44100.0;
    float gainSmoothed = 1.0f;

    void ensureStateSize();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DYNRMSRiderAudioProcessor)
};

class DYNRMSRiderAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit DYNRMSRiderAudioProcessorEditor (DYNRMSRiderAudioProcessor&);
    ~DYNRMSRiderAudioProcessorEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    DYNRMSRiderAudioProcessor& processorRef;

    juce::Slider targetLevelSlider;
    juce::Slider speedSlider;
    juce::Slider rangeSlider;
    juce::Slider hfSensitivitySlider;
    juce::Slider lookaheadSlider;
    juce::Slider outputTrimSlider;

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::vector<std::unique_ptr<SliderAttachment>> attachments;

    void initSlider (juce::Slider& slider, const juce::String& label);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DYNRMSRiderAudioProcessorEditor)
};
