#pragma once

#include <JuceHeader.h>

class EQFormSetAudioProcessor : public juce::AudioProcessor
{
public:
    EQFormSetAudioProcessor();
    ~EQFormSetAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "EQFormSet"; }
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
    struct FormantFilter
    {
        juce::dsp::IIR::Filter<float> filter;
        float phase = 0.0f;
    };

    std::vector<FormantFilter> formantFilters;
    juce::AudioBuffer<float> dryBuffer;
    double currentSampleRate = 44100.0;
    juce::uint32 lastBlockSize = 512;

    void ensureStateSize (int numChannels);
    void updateFormantFilters (float baseFreq, float width, float movement);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EQFormSetAudioProcessor)
};

class EQFormSetAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit EQFormSetAudioProcessorEditor (EQFormSetAudioProcessor&);
    ~EQFormSetAudioProcessorEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    EQFormSetAudioProcessor& processorRef;

    juce::Slider formantFreqSlider;
    juce::Slider formantWidthSlider;
    juce::Slider movementSlider;
    juce::Slider intensitySlider;
    juce::Slider mixSlider;

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::vector<std::unique_ptr<SliderAttachment>> attachments;

    void initSlider (juce::Slider& slider, const juce::String& label);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EQFormSetAudioProcessorEditor)
};
