#pragma once

#include <JuceHeader.h>

class EQTiltLineAudioProcessor : public juce::AudioProcessor
{
public:
    EQTiltLineAudioProcessor();
    ~EQTiltLineAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "EQTiltLine"; }
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
    double currentSampleRate = 44100.0;
    juce::uint32 lastBlockSize = 512;

    void ensureFilterState (int numChannels);
    void updateShelves (float pivotFreq, float lowGainDb, float highGainDb);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EQTiltLineAudioProcessor)
};

class EQTiltLineAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit EQTiltLineAudioProcessorEditor (EQTiltLineAudioProcessor&);
    ~EQTiltLineAudioProcessorEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    EQTiltLineAudioProcessor& processorRef;

    juce::Slider tiltSlider;
    juce::Slider pivotSlider;
    juce::Slider lowShelfSlider;
    juce::Slider highShelfSlider;
    juce::Slider outputSlider;

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::vector<std::unique_ptr<SliderAttachment>> attachments;

    void initSlider (juce::Slider& slider, const juce::String& label);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EQTiltLineAudioProcessorEditor)
};
