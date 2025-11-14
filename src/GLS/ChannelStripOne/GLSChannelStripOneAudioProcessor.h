#pragma once

#include <JuceHeader.h>

class GLSChannelStripOneAudioProcessor : public juce::AudioProcessor
{
public:
    GLSChannelStripOneAudioProcessor();
    ~GLSChannelStripOneAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "GLSChannelStripOne"; }
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
        juce::dsp::IIR::Filter<float> lowMidBell;
        juce::dsp::IIR::Filter<float> highMidBell;
        juce::dsp::IIR::Filter<float> highShelf;
        float gateEnvelope = 0.0f;
        float gateGain = 1.0f;
        float compEnvelope = 0.0f;
        float compGain = 1.0f;
    };

    std::vector<ChannelState> channelStates;
    juce::AudioBuffer<float> dryBuffer;
    double currentSampleRate = 44100.0;

    void ensureStateSize();
    void updateEqCoefficients (ChannelState& state,
                               float lowGain, float lowMidGain,
                               float highMidGain, float highGain);
    static float softClip (float input, float amount);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GLSChannelStripOneAudioProcessor)
};

class GLSChannelStripOneAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit GLSChannelStripOneAudioProcessorEditor (GLSChannelStripOneAudioProcessor&);
    ~GLSChannelStripOneAudioProcessorEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    GLSChannelStripOneAudioProcessor& processorRef;

    juce::Slider gateThreshSlider;
    juce::Slider gateRangeSlider;
    juce::Slider compThreshSlider;
    juce::Slider compRatioSlider;
    juce::Slider compAttackSlider;
    juce::Slider compReleaseSlider;
    juce::Slider lowGainSlider;
    juce::Slider lowMidGainSlider;
    juce::Slider highMidGainSlider;
    juce::Slider highGainSlider;
    juce::Slider satAmountSlider;
    juce::Slider mixSlider;

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::vector<std::unique_ptr<SliderAttachment>> attachments;

    void initialiseSlider (juce::Slider& slider, const juce::String& name);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GLSChannelStripOneAudioProcessorEditor)
};
