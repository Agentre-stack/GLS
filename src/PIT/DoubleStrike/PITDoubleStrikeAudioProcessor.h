#pragma once

#include <JuceHeader.h>
#include "../../DualPrecisionAudioProcessor.h"
#include "../common/SimplePitchShifter.h"

class PITDoubleStrikeAudioProcessor : public DualPrecisionAudioProcessor
{
public:
    PITDoubleStrikeAudioProcessor();
    ~PITDoubleStrikeAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "PITDoubleStrike"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int index) override
    {
        return index == 0 ? juce::String ("PIT Double Strike 01") : juce::String();
    }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getValueTreeState() { return apvts; }
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

private:
    juce::AudioProcessorValueTreeState apvts;
    double currentSampleRate = 44100.0;

    juce::AudioBuffer<float> dryBuffer;
    juce::AudioBuffer<float> wetBuffer;
    juce::AudioBuffer<float> voiceABuffer;
    juce::AudioBuffer<float> voiceBBuffer;
    pit::SimplePitchShifter voiceAShifter;
    pit::SimplePitchShifter voiceBShifter;
    std::vector<juce::dsp::IIR::Filter<float>> hpfFilters;
    std::vector<juce::dsp::IIR::Filter<float>> lpfFilters;

    void ensureState (int numChannels, int numSamples);
    void updateFilters (float hpf, float lpf);
    std::pair<float, float> panToGains (float pan) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PITDoubleStrikeAudioProcessor)
};

class PITDoubleStrikeAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit PITDoubleStrikeAudioProcessorEditor (PITDoubleStrikeAudioProcessor&);
    ~PITDoubleStrikeAudioProcessorEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    PITDoubleStrikeAudioProcessor& processorRef;

    juce::Slider voiceASlider;
    juce::Slider voiceBSlider;
    juce::Slider detuneSlider;
    juce::Slider spreadSlider;
    juce::Slider hpfSlider;
    juce::Slider lpfSlider;
    juce::Slider mixSlider;

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::vector<std::unique_ptr<SliderAttachment>> attachments;

    void initSlider (juce::Slider& slider, const juce::String& label);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PITDoubleStrikeAudioProcessorEditor)
};
