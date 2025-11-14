#pragma once

#include <JuceHeader.h>

class MDLVibeMorphAudioProcessor : public juce::AudioProcessor
{
public:
    MDLVibeMorphAudioProcessor();
    ~MDLVibeMorphAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "MDLVibeMorph"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 1.0; }

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

    struct Stage
    {
        juce::dsp::IIR::Filter<float> filter;
    };

    std::vector<std::vector<Stage>> channelStages;
    juce::AudioBuffer<float> dryBuffer;
    double currentSampleRate = 44100.0;

    std::array<float, 2> lfoPhase { 0.0f, 0.5f };

    void ensureStageState (int numChannels, int numStages);
    void updateStageCoefficients (float rate, float depth, float throb, int mode);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MDLVibeMorphAudioProcessor)
};

class MDLVibeMorphAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit MDLVibeMorphAudioProcessorEditor (MDLVibeMorphAudioProcessor&);
    ~MDLVibeMorphAudioProcessorEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    MDLVibeMorphAudioProcessor& processorRef;

    juce::Slider rateSlider;
    juce::Slider depthSlider;
    juce::Slider throbSlider;
    juce::Slider mixSlider;
    juce::ComboBox modeBox;

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboAttachment  = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    std::vector<std::unique_ptr<SliderAttachment>> sliderAttachments;
    std::unique_ptr<ComboAttachment> modeAttachment;

    void initSlider (juce::Slider&, const juce::String&);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MDLVibeMorphAudioProcessorEditor)
};
