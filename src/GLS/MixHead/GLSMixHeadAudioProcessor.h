#pragma once

#include <JuceHeader.h>
#include "../../DualPrecisionAudioProcessor.h"

class GLSMixHeadAudioProcessor : public DualPrecisionAudioProcessor
{
public:
    GLSMixHeadAudioProcessor();
    ~GLSMixHeadAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "GLSMixHead"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int index) override
    {
        return index == 0 ? juce::String ("GLS Mix Head 01") : juce::String();
    }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getValueTreeState() { return apvts; }
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

private:
    juce::AudioProcessorValueTreeState apvts;
    struct ChannelState
    {
        float toneLowState = 0.0f;
    };

    std::vector<ChannelState> channelStates;
    double currentSampleRate = 44100.0;
    float toneSmoothingCoeff = 0.0f;

    void ensureStateSize();
    float processTone (ChannelState& state, float sample) const;
    static float applySaturation (float sample, float drive);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GLSMixHeadAudioProcessor)
};

class GLSMixHeadAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit GLSMixHeadAudioProcessorEditor (GLSMixHeadAudioProcessor&);
    ~GLSMixHeadAudioProcessorEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    GLSMixHeadAudioProcessor& processorRef;

    juce::Slider driveSlider;
    juce::Slider headroomSlider;
    juce::Slider toneSlider;
    juce::Slider widthSlider;
    juce::Slider outputTrimSlider;

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::vector<std::unique_ptr<SliderAttachment>> attachments;

    void initialiseSlider (juce::Slider& slider, const juce::String& name);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GLSMixHeadAudioProcessorEditor)
};
