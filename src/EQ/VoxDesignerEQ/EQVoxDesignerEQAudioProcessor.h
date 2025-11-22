#pragma once

#include <JuceHeader.h>
#include "../../DualPrecisionAudioProcessor.h"

class EQVoxDesignerEQAudioProcessor : public DualPrecisionAudioProcessor
{
public:
    EQVoxDesignerEQAudioProcessor();
    ~EQVoxDesignerEQAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "EQVoxDesignerEQ"; }
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
    std::vector<juce::dsp::IIR::Filter<float>> chestShelves;
    std::vector<juce::dsp::IIR::Filter<float>> presenceBells;
    std::vector<juce::dsp::IIR::Filter<float>> sibilanceFilters;
    std::vector<juce::dsp::IIR::Filter<float>> airShelves;
    std::vector<juce::dsp::IIR::Filter<float>> exciterHighpasses;
    std::vector<float> sibilanceEnvelopes;
    juce::AudioBuffer<float> dryBuffer;
    double currentSampleRate = 44100.0;
    juce::uint32 lastBlockSize = 512;

    void ensureStateSize (int numChannels);
    void updateFilters (float chestGain, float presenceGain, float airGain);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EQVoxDesignerEQAudioProcessor)
};

class EQVoxDesignerEQAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit EQVoxDesignerEQAudioProcessorEditor (EQVoxDesignerEQAudioProcessor&);
    ~EQVoxDesignerEQAudioProcessorEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    EQVoxDesignerEQAudioProcessor& processorRef;

    juce::Slider chestSlider;
    juce::Slider presenceSlider;
    juce::Slider sibilanceSlider;
    juce::Slider airSlider;
    juce::Slider exciterSlider;

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::vector<std::unique_ptr<SliderAttachment>> attachments;

    void initSlider (juce::Slider& slider, const juce::String& label);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EQVoxDesignerEQAudioProcessorEditor)
};
