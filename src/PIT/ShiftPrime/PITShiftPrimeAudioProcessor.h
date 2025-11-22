#pragma once

#include <JuceHeader.h>
#include "../../DualPrecisionAudioProcessor.h"
#include "../common/SimplePitchShifter.h"

class PITShiftPrimeAudioProcessor : public DualPrecisionAudioProcessor
{
public:
    PITShiftPrimeAudioProcessor();
    ~PITShiftPrimeAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "PITShiftPrime"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int index) override
    {
        return index == 0 ? juce::String ("PIT Shift Prime 01") : juce::String();
    }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getValueTreeState() { return apvts; }
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

private:
    juce::AudioProcessorValueTreeState apvts;
    double currentSampleRate = 44100.0;

    std::vector<juce::dsp::IIR::Filter<float>> hpfFilters;
    std::vector<juce::dsp::IIR::Filter<float>> lpfFilters;
    std::vector<juce::dsp::IIR::Filter<float>> formantFilters;

    juce::AudioBuffer<float> dryBuffer;
    juce::AudioBuffer<float> wetBuffer;
    pit::SimplePitchShifter pitchShifter;

    void ensureStateSize (int numChannels, int numSamples);
    void updateFilters (float hpf, float lpf, float formant);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PITShiftPrimeAudioProcessor)
};

class PITShiftPrimeAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit PITShiftPrimeAudioProcessorEditor (PITShiftPrimeAudioProcessor&);
    ~PITShiftPrimeAudioProcessorEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    PITShiftPrimeAudioProcessor& processorRef;

    juce::Slider semitoneSlider;
    juce::Slider centsSlider;
    juce::Slider formantSlider;
    juce::Slider hpfSlider;
    juce::Slider lpfSlider;
    juce::ComboBox modeBox;
    juce::Slider mixSlider;

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboAttachment  = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    std::vector<std::unique_ptr<SliderAttachment>> sliderAttachments;
    std::unique_ptr<ComboAttachment> modeAttachment;

    void initSlider (juce::Slider& slider, const juce::String& label);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PITShiftPrimeAudioProcessorEditor)
};
