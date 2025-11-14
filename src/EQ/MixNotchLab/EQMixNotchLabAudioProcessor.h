#pragma once

#include <JuceHeader.h>

class EQMixNotchLabAudioProcessor : public juce::AudioProcessor
{
public:
    EQMixNotchLabAudioProcessor();
    ~EQMixNotchLabAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "EQMixNotchLab"; }
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
    std::vector<juce::dsp::IIR::Filter<float>> notch1Filters;
    std::vector<juce::dsp::IIR::Filter<float>> notch2Filters;
    std::vector<juce::dsp::IIR::Filter<float>> notch1PreviewFilters;
    std::vector<juce::dsp::IIR::Filter<float>> notch2PreviewFilters;
    juce::AudioBuffer<float> dryBuffer;
    juce::AudioBuffer<float> notchPreview1;
    juce::AudioBuffer<float> notchPreview2;
    double currentSampleRate = 44100.0;
    juce::uint32 lastBlockSize = 512;

    void ensureStateSize (int numChannels);
    void updateFilters (float n1Freq, float n1Q, float n1Depth,
                        float n2Freq, float n2Q, float n2Depth);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EQMixNotchLabAudioProcessor)
};

class EQMixNotchLabAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit EQMixNotchLabAudioProcessorEditor (EQMixNotchLabAudioProcessor&);
    ~EQMixNotchLabAudioProcessorEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    EQMixNotchLabAudioProcessor& processorRef;

    juce::Slider notch1FreqSlider;
    juce::Slider notch1QSlider;
    juce::Slider notch1DepthSlider;
    juce::Slider notch2FreqSlider;
    juce::Slider notch2QSlider;
    juce::Slider notch2DepthSlider;
    juce::ComboBox listenModeBox;

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboAttachment  = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    std::vector<std::unique_ptr<SliderAttachment>> sliderAttachments;
    std::unique_ptr<ComboAttachment> listenModeAttachment;

    void initSlider (juce::Slider& slider, const juce::String& label);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EQMixNotchLabAudioProcessorEditor)
};
