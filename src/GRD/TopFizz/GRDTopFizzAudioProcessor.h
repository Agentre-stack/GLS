#pragma once

#include <JuceHeader.h>

class GRDTopFizzAudioProcessor : public juce::AudioProcessor
{
public:
    GRDTopFizzAudioProcessor();
    ~GRDTopFizzAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "GRDTopFizz"; }
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
    double currentSampleRate = 44100.0;
    std::vector<juce::dsp::IIR::Filter<float>> highBandFilters;
    std::vector<juce::dsp::IIR::Filter<float>> smoothingFilters;
    juce::AudioBuffer<float> dryBuffer;
    juce::uint32 lastBlockSize = 0;

    void ensureStateSize (int numChannels, int numSamples);
    void updateFilters (float bandFreq, float smoothFreq);
    float generateHarmonics (float input, float amount, float blend) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GRDTopFizzAudioProcessor)
};

class GRDTopFizzAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit GRDTopFizzAudioProcessorEditor (GRDTopFizzAudioProcessor&);
    ~GRDTopFizzAudioProcessorEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    GRDTopFizzAudioProcessor& processorRef;

    juce::Slider freqSlider;
    juce::Slider amountSlider;
    juce::Slider oddEvenSlider;
    juce::Slider deHarshSlider;
    juce::Slider mixSlider;

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::vector<std::unique_ptr<SliderAttachment>> attachments;

    void initSlider (juce::Slider& slider, const juce::String& label);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GRDTopFizzAudioProcessorEditor)
};
