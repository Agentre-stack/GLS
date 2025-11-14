#pragma once

#include <JuceHeader.h>

class EQDynamicTiltProAudioProcessor : public juce::AudioProcessor
{
public:
    EQDynamicTiltProAudioProcessor();
    ~EQDynamicTiltProAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "EQDynamicTiltPro"; }
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
    std::vector<juce::dsp::IIR::Filter<float>> lowShelves;
    std::vector<juce::dsp::IIR::Filter<float>> highShelves;
    std::vector<float> envelopes;
    double currentSampleRate = 44100.0;
    juce::uint32 lastBlockSize = 512;

    void ensureStateSize (int numChannels);
    void updateFilters (float totalTiltDb, float pivotFreq);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EQDynamicTiltProAudioProcessor)
};

class EQDynamicTiltProAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit EQDynamicTiltProAudioProcessorEditor (EQDynamicTiltProAudioProcessor&);
    ~EQDynamicTiltProAudioProcessorEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    EQDynamicTiltProAudioProcessor& processorRef;

    juce::Slider tiltSlider;
    juce::Slider pivotSlider;
    juce::Slider threshSlider;
    juce::Slider rangeSlider;
    juce::Slider attackSlider;
    juce::Slider releaseSlider;
    juce::Slider outputSlider;

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::vector<std::unique_ptr<SliderAttachment>> attachments;

    void initSlider (juce::Slider& slider, const juce::String& label);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EQDynamicTiltProAudioProcessorEditor)
};
