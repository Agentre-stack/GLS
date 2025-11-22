#pragma once

#include <JuceHeader.h>
#include "../../DualPrecisionAudioProcessor.h"

class EQInfraSculptAudioProcessor : public DualPrecisionAudioProcessor
{
public:
    EQInfraSculptAudioProcessor();
    ~EQInfraSculptAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "EQInfraSculpt"; }
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
    struct HighPassStack
    {
        std::vector<juce::dsp::IIR::Filter<float>> stages;
    };

    std::vector<HighPassStack> hpStacks;
    std::vector<juce::dsp::IIR::Filter<float>> resonanceFilters;
    std::vector<juce::dsp::IIR::Filter<float>> monoLowFilters;
    juce::AudioBuffer<float> monoBuffer;
    double currentSampleRate = 44100.0;
    juce::uint32 lastBlockSize = 512;
    int activeStageCount = 4;

    void ensureStateSize (int numChannels, int stageCount);
    void updateFilters (float subHpf, int stageCount, float resonance, float monoBelow);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EQInfraSculptAudioProcessor)
};

class EQInfraSculptAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit EQInfraSculptAudioProcessorEditor (EQInfraSculptAudioProcessor&);
    ~EQInfraSculptAudioProcessorEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    EQInfraSculptAudioProcessor& processorRef;

    juce::Slider subHpfSlider;
    juce::Slider infraSlopeSlider;
    juce::Slider subResonanceSlider;
    juce::Slider monoBelowSlider;
    juce::Slider outputTrimSlider;

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::vector<std::unique_ptr<SliderAttachment>> attachments;

    void initSlider (juce::Slider& slider, const juce::String& label);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EQInfraSculptAudioProcessorEditor)
};
