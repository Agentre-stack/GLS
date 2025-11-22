#pragma once

#include <JuceHeader.h>
#include "../../DualPrecisionAudioProcessor.h"
#include "../common/SimplePitchShifter.h"

class PITGrowlWarpAudioProcessor : public DualPrecisionAudioProcessor
{
public:
    PITGrowlWarpAudioProcessor();
    ~PITGrowlWarpAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "PITGrowlWarp"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int index) override
    {
        return index == 0 ? juce::String ("PIT Growl Warp 01") : juce::String();
    }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getValueTreeState() { return apvts; }
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

private:
    juce::AudioProcessorValueTreeState apvts;

    juce::AudioBuffer<float> dryBuffer;
    juce::AudioBuffer<float> wetBuffer;
    pit::SimplePitchShifter pitchShifter;
    std::vector<juce::dsp::IIR::Filter<float>> formantFilters;
    double currentSampleRate = 44100.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PITGrowlWarpAudioProcessor)
};

//==============================================================================
class PITGrowlWarpAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit PITGrowlWarpAudioProcessorEditor (PITGrowlWarpAudioProcessor&);
    ~PITGrowlWarpAudioProcessorEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    PITGrowlWarpAudioProcessor& processorRef;

    juce::Slider semitonesDownSlider;
    juce::Slider growlSlider;
    juce::Slider formantSlider;
    juce::Slider driveSlider;
    juce::Slider mixSlider;

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::vector<std::unique_ptr<SliderAttachment>> attachments;

    void initSlider (juce::Slider& slider, const juce::String& labelText);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PITGrowlWarpAudioProcessorEditor)
};
