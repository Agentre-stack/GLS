#pragma once

#include <JuceHeader.h>
#include "../../DualPrecisionAudioProcessor.h"

class MDLTempoLFOAudioProcessor : public DualPrecisionAudioProcessor
{
public:
    MDLTempoLFOAudioProcessor();
    ~MDLTempoLFOAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "MDLTempoLFO"; }
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

    float lfoPhase = 0.0f;
    float smoothedValue = 0.0f;
    double currentSampleRate = 44100.0;
    double bpm = 120.0;

    float getSyncRate() const;
    float getWaveValue (float phase, int shape) const;
    void refreshTempoFromHost();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MDLTempoLFOAudioProcessor)
};

class MDLTempoLFOAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit MDLTempoLFOAudioProcessorEditor (MDLTempoLFOAudioProcessor&);
    ~MDLTempoLFOAudioProcessorEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    MDLTempoLFOAudioProcessor& processorRef;

    juce::Slider depthSlider;
    juce::Slider offsetSlider;
    juce::Slider smoothingSlider;
    juce::ComboBox shapeBox;
    juce::ComboBox syncBox;

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboAttachment  = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    std::vector<std::unique_ptr<SliderAttachment>> sliderAttachments;
    std::unique_ptr<ComboAttachment> shapeAttachment;
    std::unique_ptr<ComboAttachment> syncAttachment;

    void initSlider (juce::Slider&, const juce::String&);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MDLTempoLFOAudioProcessorEditor)
};
