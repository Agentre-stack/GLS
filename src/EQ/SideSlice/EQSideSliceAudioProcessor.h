#pragma once

#include <JuceHeader.h>
#include "../../DualPrecisionAudioProcessor.h"
#include <array>

class EQSideSliceAudioProcessor : public DualPrecisionAudioProcessor
{
public:
    EQSideSliceAudioProcessor();
    ~EQSideSliceAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "EQSideSlice"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int index) override { return index == 0 ? juce::String (JucePlugin_Name " 01") : juce::String(); }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getValueTreeState() { return apvts; }
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

private:
    juce::AudioProcessorValueTreeState apvts;
    juce::dsp::IIR::Filter<float> midFilter;
    juce::dsp::IIR::Filter<float> sideFilter;
    std::array<juce::dsp::IIR::Filter<float>, 2> stereoFilters;
    double currentSampleRate = 44100.0;
    juce::uint32 lastBlockSize = 512;

    void prepareFilters (double sampleRate, int samplesPerBlock);
    void updateFilterCoefficients (float midBandDb, float sideBandDb);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EQSideSliceAudioProcessor)
};

class EQSideSliceAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit EQSideSliceAudioProcessorEditor (EQSideSliceAudioProcessor&);
    ~EQSideSliceAudioProcessorEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    EQSideSliceAudioProcessor& processorRef;

    juce::ComboBox modeBox;
    juce::Slider midBandSlider;
    juce::Slider sideBandSlider;
    juce::Slider midTrimSlider;
    juce::Slider sideTrimSlider;
    juce::Slider widthSlider;

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboBoxAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    std::unique_ptr<ComboBoxAttachment> modeAttachment;
    std::vector<std::unique_ptr<SliderAttachment>> sliderAttachments;

    void initSlider (juce::Slider& slider, const juce::String& label);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EQSideSliceAudioProcessorEditor)
};
