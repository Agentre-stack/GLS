#pragma once

#include <JuceHeader.h>

class GRDFaultLineFuzzAudioProcessor : public juce::AudioProcessor
{
public:
    GRDFaultLineFuzzAudioProcessor();
    ~GRDFaultLineFuzzAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "GRDFaultLineFuzz"; }
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
    juce::AudioBuffer<float> processingBuffer;
    std::vector<juce::dsp::IIR::Filter<float>> toneFilters;
    std::vector<float> gateState;

    void ensureStateSize (int numChannels, int numSamples);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GRDFaultLineFuzzAudioProcessor)
};

class GRDFaultLineFuzzAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit GRDFaultLineFuzzAudioProcessorEditor (GRDFaultLineFuzzAudioProcessor&);
    ~GRDFaultLineFuzzAudioProcessorEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    GRDFaultLineFuzzAudioProcessor& processorRef;

    juce::Slider inputTrimSlider;
    juce::Slider fuzzSlider;
    juce::Slider biasSlider;
    juce::Slider gateSlider;
    juce::Slider toneSlider;
    juce::Slider outputTrimSlider;

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::vector<std::unique_ptr<SliderAttachment>> attachments;

    void initSlider (juce::Slider& slider, const juce::String& name);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GRDFaultLineFuzzAudioProcessorEditor)
};
