#pragma once

#include <JuceHeader.h>

class DYNTransFixAudioProcessor : public juce::AudioProcessor
{
public:
    DYNTransFixAudioProcessor();
    ~DYNTransFixAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "DYNTransFix"; }
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
        float detector = 0.0f;
        float attackEnv = 0.0f;
        float sustainEnv = 0.0f;
        juce::dsp::IIR::Filter<float> hfFilter;
        juce::dsp::IIR::Filter<float> lfFilter;
    };

    std::vector<ChannelState> channelStates;
    double currentSampleRate = 44100.0;
    juce::AudioBuffer<float> dryBuffer;

    void ensureStateSize();
    float applyTilt (float sample, float freq, float amount);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DYNTransFixAudioProcessor)
};

class DYNTransFixAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit DYNTransFixAudioProcessorEditor (DYNTransFixAudioProcessor&);
    ~DYNTransFixAudioProcessorEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    DYNTransFixAudioProcessor& processorRef;

    juce::Slider attackSlider;
    juce::Slider sustainSlider;
    juce::Slider tiltFreqSlider;
    juce::Slider tiltAmountSlider;
    juce::ComboBox detectModeBox;
    juce::Slider mixSlider;

    using SliderAttachment   = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboBoxAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    std::unique_ptr<SliderAttachment> attackAttachment;
    std::unique_ptr<SliderAttachment> sustainAttachment;
    std::unique_ptr<SliderAttachment> tiltFreqAttachment;
    std::unique_ptr<SliderAttachment> tiltAmountAttachment;
    std::unique_ptr<ComboBoxAttachment> detectModeAttachment;
    std::unique_ptr<SliderAttachment> mixAttachment;

    void initSlider (juce::Slider& slider, const juce::String& label);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DYNTransFixAudioProcessorEditor)
};
