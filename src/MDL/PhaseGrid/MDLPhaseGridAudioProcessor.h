#pragma once

#include <JuceHeader.h>
#include "../../DualPrecisionAudioProcessor.h"

class MDLPhaseGridAudioProcessor : public DualPrecisionAudioProcessor
{
public:
    MDLPhaseGridAudioProcessor();
    ~MDLPhaseGridAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "MDLPhaseGrid"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 1.0; }

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

    struct AllPassStage
    {
        juce::dsp::IIR::Filter<float> filter;
    };

    std::vector<std::vector<AllPassStage>> channelStages;
    double currentSampleRate = 44100.0;
    juce::uint32 lastBlockSize = 512;
    std::vector<float> lfoPhase;
    double stageSpecSampleRate = 0.0;
    juce::uint32 stageSpecBlockSize = 0;

    void ensureStageState (int numChannels, int numStages);
    void updateStageCoefficients (float centreFreq, float depth, float rate);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MDLPhaseGridAudioProcessor)
};

class MDLPhaseGridAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit MDLPhaseGridAudioProcessorEditor (MDLPhaseGridAudioProcessor&);
    ~MDLPhaseGridAudioProcessorEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    MDLPhaseGridAudioProcessor& processorRef;

    juce::Slider stagesSlider;
    juce::Slider centerSlider;
    juce::Slider rateSlider;
    juce::Slider depthSlider;
    juce::Slider feedbackSlider;
    juce::Slider mixSlider;

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::vector<std::unique_ptr<SliderAttachment>> attachments;

    void initSlider (juce::Slider&, const juce::String&);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MDLPhaseGridAudioProcessorEditor)
};
