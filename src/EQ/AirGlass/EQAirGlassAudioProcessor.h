#pragma once

#include <JuceHeader.h>

class EQAirGlassAudioProcessor : public juce::AudioProcessor
{
public:
    EQAirGlassAudioProcessor();
    ~EQAirGlassAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "EQAirGlass"; }
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
    std::vector<juce::dsp::IIR::Filter<float>> airShelves;
    std::vector<juce::dsp::IIR::Filter<float>> harshFilters;
    std::vector<float> harshEnvelopes;
    double currentSampleRate = 44100.0;
    juce::uint32 lastBlockSize = 512;

    void ensureStateSize (int numChannels);
    void updateShelfCoefficients (float freq, float gainDb);
    void updateHarshFilters (float freq);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EQAirGlassAudioProcessor)
};

class EQAirGlassAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit EQAirGlassAudioProcessorEditor (EQAirGlassAudioProcessor&);
    ~EQAirGlassAudioProcessorEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    EQAirGlassAudioProcessor& processorRef;

    juce::Slider airFreqSlider;
    juce::Slider airGainSlider;
    juce::Slider harmonicBlendSlider;
    juce::Slider deHarshSlider;
    juce::Slider outputTrimSlider;

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::vector<std::unique_ptr<SliderAttachment>> attachments;

    void initSlider (juce::Slider& slider, const juce::String& label);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EQAirGlassAudioProcessorEditor)
};
