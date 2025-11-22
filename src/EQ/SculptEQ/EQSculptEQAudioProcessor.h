#pragma once

#include <JuceHeader.h>
#include "../../DualPrecisionAudioProcessor.h"

class EQSculptEQAudioProcessor : public DualPrecisionAudioProcessor
{
public:
    EQSculptEQAudioProcessor();
    ~EQSculptEQAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "EQSculptEQ"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int index) override { return index == 0 ? juce::String (JucePlugin_Name " 01") : juce::String(); }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getValueTreeState() { return apvts; }
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

private:
    juce::AudioProcessorValueTreeState apvts;
    std::vector<juce::dsp::IIR::Filter<float>> highPassFilters;
    std::vector<juce::dsp::IIR::Filter<float>> lowPassFilters;
    std::array<std::vector<juce::dsp::IIR::Filter<float>>, 6> bandFilters;
    double currentSampleRate = 44100.0;
    juce::uint32 lastBlockSize = 512;

    void ensureFilterState (int numChannels);
    void updateFilters (float hpf, float lpf,
                        const std::array<float, 6>& freqs,
                        const std::array<float, 6>& gains,
                        const std::array<float, 6>& qs);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EQSculptEQAudioProcessor)
};

class EQSculptEQAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit EQSculptEQAudioProcessorEditor (EQSculptEQAudioProcessor&);
    ~EQSculptEQAudioProcessorEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    EQSculptEQAudioProcessor& processorRef;

    juce::Slider hpfSlider;
    juce::Slider lpfSlider;
    std::array<juce::Slider, 6> freqSliders;
    std::array<juce::Slider, 6> gainSliders;
    std::array<juce::Slider, 6> qSliders;

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::vector<std::unique_ptr<SliderAttachment>> attachments;

    void initSlider (juce::Slider& slider, const juce::String& label);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EQSculptEQAudioProcessorEditor)
};
