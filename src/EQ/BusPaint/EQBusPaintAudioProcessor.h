#pragma once

#include <JuceHeader.h>

class EQBusPaintAudioProcessor : public juce::AudioProcessor
{
public:
    EQBusPaintAudioProcessor();
    ~EQBusPaintAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "EQBusPaint"; }
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
    std::vector<juce::dsp::IIR::Filter<float>> presenceBells;
    std::vector<juce::dsp::IIR::Filter<float>> warmthBells;
    double currentSampleRate = 44100.0;
    juce::uint32 lastBlockSize = 512;

    void ensureFilterState (int numChannels);
    void updateFilters (float lowTilt, float highTilt, float presence, float warmth);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EQBusPaintAudioProcessor)
};

class EQBusPaintAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit EQBusPaintAudioProcessorEditor (EQBusPaintAudioProcessor&);
    ~EQBusPaintAudioProcessorEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    EQBusPaintAudioProcessor& processorRef;

    juce::Slider lowTiltSlider;
    juce::Slider highTiltSlider;
    juce::Slider presenceSlider;
    juce::Slider warmthSlider;
    juce::Slider outputSlider;

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::vector<std::unique_ptr<SliderAttachment>> attachments;

    void initSlider (juce::Slider& slider, const juce::String& label);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EQBusPaintAudioProcessorEditor)
};
