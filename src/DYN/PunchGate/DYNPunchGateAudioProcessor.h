#pragma once

#include <JuceHeader.h>

class DYNPunchGateAudioProcessor : public juce::AudioProcessor
{
public:
    DYNPunchGateAudioProcessor();
    ~DYNPunchGateAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "DYNPunchGate"; }
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
        float envelope = 0.0f;
        float holdCounter = 0.0f;
        float gateGain = 1.0f;
    };

    std::vector<ChannelState> channelStates;
    double currentSampleRate = 44100.0;

    void ensureStateSize();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DYNPunchGateAudioProcessor)
};

class DYNPunchGateAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit DYNPunchGateAudioProcessorEditor (DYNPunchGateAudioProcessor&);
    ~DYNPunchGateAudioProcessorEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    DYNPunchGateAudioProcessor& processorRef;

    juce::Slider threshSlider;
    juce::Slider rangeSlider;
    juce::Slider attackSlider;
    juce::Slider holdSlider;
    juce::Slider releaseSlider;
    juce::Slider hysteresisSlider;
    juce::Slider punchBoostSlider;

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::vector<std::unique_ptr<SliderAttachment>> attachments;

    void initialiseSlider (juce::Slider& slider, const juce::String& label);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DYNPunchGateAudioProcessorEditor)
};
