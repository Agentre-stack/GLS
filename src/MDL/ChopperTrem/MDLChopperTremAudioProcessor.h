#pragma once

#include <JuceHeader.h>
#include "../../DualPrecisionAudioProcessor.h"

class MDLChopperTremAudioProcessor : public DualPrecisionAudioProcessor
{
public:
    MDLChopperTremAudioProcessor();
    ~MDLChopperTremAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "MDLChopperTrem"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int index) override
    {
        return index == 0 ? juce::String ("MDL Chopper Trem 01") : juce::String();
    }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getValueTreeState() { return apvts; }
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

private:
    juce::AudioProcessorValueTreeState apvts;

    std::array<float, 64> pattern {};
    float phase = 0.0f;
    double currentSampleRate = 44100.0;
    double bpm = 120.0;

    juce::AudioBuffer<float> dryBuffer;
    juce::AudioBuffer<float> doublePrecisionBuffer;

    void rebuildPattern();
    void refreshTempoFromHost();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MDLChopperTremAudioProcessor)
};

class MDLChopperTremAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit MDLChopperTremAudioProcessorEditor (MDLChopperTremAudioProcessor&);
    ~MDLChopperTremAudioProcessorEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    MDLChopperTremAudioProcessor& processorRef;

    juce::Slider depthSlider;
    juce::Slider rateSlider;
    juce::Slider smoothSlider;
    juce::Slider hpfSlider;
    juce::Slider mixSlider;
    juce::ComboBox patternBox;

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboAttachment  = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    std::vector<std::unique_ptr<SliderAttachment>> sliderAttachments;
    std::unique_ptr<ComboAttachment> patternAttachment;

    void initSlider (juce::Slider&, const juce::String&);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MDLChopperTremAudioProcessorEditor)
};
