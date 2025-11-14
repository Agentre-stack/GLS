#pragma once

#include <JuceHeader.h>

class DYNSmoothDestroyerAudioProcessor : public juce::AudioProcessor
{
public:
    DYNSmoothDestroyerAudioProcessor();
    ~DYNSmoothDestroyerAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "DYNSmoothDestroyer"; }
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
        juce::dsp::IIR::Filter<float> bandFilter;
        float envelope = 0.0f;
        float gain = 1.0f;
    };

    std::vector<DynamicBand> band1States;
    std::vector<DynamicBand> band2States;
    juce::AudioBuffer<float> dryBuffer;
    double currentSampleRate = 44100.0;

    void ensureStateSize();
    void updateBandCoefficients (DynamicBand& band, float freq, float q);
    float computeBandGain (float levelDb, float threshDb, float rangeDb) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DYNSmoothDestroyerAudioProcessor)
};

class DYNSmoothDestroyerAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit DYNSmoothDestroyerAudioProcessorEditor (DYNSmoothDestroyerAudioProcessor&);
    ~DYNSmoothDestroyerAudioProcessorEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    DYNSmoothDestroyerAudioProcessor& processorRef;

    juce::Slider band1FreqSlider;
    juce::Slider band1QSlider;
    juce::Slider band1ThreshSlider;
    juce::Slider band1RangeSlider;
    juce::Slider band2FreqSlider;
    juce::Slider band2QSlider;
    juce::Slider band2ThreshSlider;
    juce::Slider band2RangeSlider;
    juce::Slider globalAttackSlider;
    juce::Slider globalReleaseSlider;
    juce::Slider mixSlider;

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::vector<std::unique_ptr<SliderAttachment>> attachments;

    void initialiseSlider (juce::Slider& slider, const juce::String& label);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DYNSmoothDestroyerAudioProcessorEditor)
};
