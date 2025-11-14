#pragma once

#include <JuceHeader.h>

class GLSSubCommandAudioProcessor : public juce::AudioProcessor
{
public:
    GLSSubCommandAudioProcessor();
    ~GLSSubCommandAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "GLSSubCommand"; }
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
    juce::AudioBuffer<float> originalBuffer;

    struct ChannelState
    {
        juce::dsp::IIR::Filter<float> lowPass;
        juce::dsp::IIR::Filter<float> outputHPF;
        float envelope = 0.0f;
        float gain = 1.0f;
    };

    std::vector<ChannelState> channelStates;

    void ensureStateSize();
    void updateFilters (ChannelState& state, float xoverFreq, float outHpfFreq);
    static float generateHarmonics (float sample, float amount);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GLSSubCommandAudioProcessor)
};

class GLSSubCommandAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit GLSSubCommandAudioProcessorEditor (GLSSubCommandAudioProcessor&);
    ~GLSSubCommandAudioProcessorEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    GLSSubCommandAudioProcessor& processorRef;

    juce::Slider xoverFreqSlider;
    juce::Slider subLevelSlider;
    juce::Slider tightnessSlider;
    juce::Slider harmonicsSlider;
    juce::Slider outHpfSlider;
    juce::Slider mixSlider;

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::vector<std::unique_ptr<SliderAttachment>> attachments;

    void initialiseSlider (juce::Slider& slider, const juce::String& name);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GLSSubCommandAudioProcessorEditor)
};
