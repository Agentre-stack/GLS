#pragma once

#include <JuceHeader.h>

class GLSStemBalancerAudioProcessor : public juce::AudioProcessor
{
public:
    GLSStemBalancerAudioProcessor();
    ~GLSStemBalancerAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "GLSStemBalancer"; }
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
        juce::dsp::IIR::Filter<float> lowShelf;
        juce::dsp::IIR::Filter<float> highShelf;
        juce::dsp::IIR::Filter<float> presenceBell;
        juce::dsp::IIR::Filter<float> lowTightHpf;
    };

    std::vector<ChannelState> channelStates;
    double currentSampleRate = 44100.0;

    void ensureStateSize();
    void updateFilters (float tilt, float presence, float lowTight);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GLSStemBalancerAudioProcessor)
};

class GLSStemBalancerAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit GLSStemBalancerAudioProcessorEditor (GLSStemBalancerAudioProcessor&);
    ~GLSStemBalancerAudioProcessorEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    GLSStemBalancerAudioProcessor& processorRef;

    juce::Slider stemGainSlider;
    juce::Slider tiltSlider;
    juce::Slider presenceSlider;
    juce::Slider lowTightSlider;
    juce::ToggleButton autoGainButton { "Auto Gain" };

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    std::unique_ptr<SliderAttachment> stemGainAttachment;
    std::unique_ptr<SliderAttachment> tiltAttachment;
    std::unique_ptr<SliderAttachment> presenceAttachment;
    std::unique_ptr<SliderAttachment> lowTightAttachment;
    std::unique_ptr<ButtonAttachment> autoGainAttachment;

    void initSlider (juce::Slider& slider, const juce::String& label);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GLSStemBalancerAudioProcessorEditor)
};
