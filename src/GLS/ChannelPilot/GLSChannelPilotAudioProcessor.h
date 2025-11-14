#pragma once

#include <JuceHeader.h>

class GLSChannelPilotAudioProcessor : public juce::AudioProcessor
{
public:
    GLSChannelPilotAudioProcessor();
    ~GLSChannelPilotAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "GLSChannelPilot"; }
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
    double currentSampleRate = 44100.0;
    std::vector<juce::dsp::IIR::Filter<float>> highPassFilters;
    std::vector<juce::dsp::IIR::Filter<float>> lowPassFilters;

    void updateFilterCoefficients (float hpfFreq, float lpfFreq);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GLSChannelPilotAudioProcessor)
};

class GLSChannelPilotAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit GLSChannelPilotAudioProcessorEditor (GLSChannelPilotAudioProcessor&);
    ~GLSChannelPilotAudioProcessorEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    GLSChannelPilotAudioProcessor& processorRef;

    juce::Slider inputTrimSlider;
    juce::Slider hpfSlider;
    juce::Slider lpfSlider;
    juce::ToggleButton phaseButton { "Phase" };
    juce::Slider panSlider;
    juce::Slider outputTrimSlider;

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    std::unique_ptr<SliderAttachment> inputTrimAttachment;
    std::unique_ptr<SliderAttachment> hpfAttachment;
    std::unique_ptr<SliderAttachment> lpfAttachment;
    std::unique_ptr<ButtonAttachment> phaseAttachment;
    std::unique_ptr<SliderAttachment> panAttachment;
    std::unique_ptr<SliderAttachment> outputTrimAttachment;

    void initialiseSlider (juce::Slider& slider, const juce::String& name);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GLSChannelPilotAudioProcessorEditor)
};
