#pragma once

#include <JuceHeader.h>

class EQDynBandAudioProcessor : public juce::AudioProcessor
{
public:
    EQDynBandAudioProcessor();
    ~EQDynBandAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "EQDynBand"; }
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
    struct DynamicBand
    {
        juce::dsp::IIR::Filter<float> filter;
        float envelope = 0.0f;
        float gain = 1.0f;
    };

    std::vector<DynamicBand> band1States;
    std::vector<DynamicBand> band2States;
    juce::AudioBuffer<float> dryBuffer;
    double currentSampleRate = 44100.0;
    juce::uint32 lastBlockSize = 512;

    void ensureStateSize (int numChannels);
    void updateBandFilters (DynamicBand& bandState, float freq, float q);
    float computeGainDb (float envDb, float threshDb, float rangeDb) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EQDynBandAudioProcessor)
};

class EQDynBandAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit EQDynBandAudioProcessorEditor (EQDynBandAudioProcessor&);
    ~EQDynBandAudioProcessorEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    EQDynBandAudioProcessor& processorRef;

    juce::Slider band1FreqSlider;
    juce::Slider band1QSlider;
    juce::Slider band1ThreshSlider;
    juce::Slider band1RangeSlider;
    juce::Slider band2FreqSlider;
    juce::Slider band2QSlider;
    juce::Slider band2ThreshSlider;
    juce::Slider band2RangeSlider;
    juce::Slider mixSlider;

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::vector<std::unique_ptr<SliderAttachment>> attachments;

    void initSlider (juce::Slider& slider, const juce::String& label);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EQDynBandAudioProcessorEditor)
};
