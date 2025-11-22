#pragma once

#include <JuceHeader.h>
#include "../../DualPrecisionAudioProcessor.h"

class MDLGhostEchoAudioProcessor : public DualPrecisionAudioProcessor
{
public:
    MDLGhostEchoAudioProcessor();
    ~MDLGhostEchoAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "MDLGhostEcho"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 4.0; }

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

    struct DiffuseTap
    {
        juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> delay { 192000 };
        juce::dsp::IIR::Filter<float> dampingFilter;
        float feedback = 0.4f;
    };

    std::vector<DiffuseTap> taps;
    juce::AudioBuffer<float> dryBuffer;
    double currentSampleRate = 44100.0;
    juce::uint32 lastBlockSize = 512;
    double tapSpecSampleRate = 0.0;
    juce::uint32 tapSpecBlockSize = 0;

    void ensureStateSize (int numChannels);
    void setTapDelayTimes (float baseTimeMs);
    void updateTapFilters (float damping);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MDLGhostEchoAudioProcessor)
};

class MDLGhostEchoAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit MDLGhostEchoAudioProcessorEditor (MDLGhostEchoAudioProcessor&);
    ~MDLGhostEchoAudioProcessorEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    MDLGhostEchoAudioProcessor& processorRef;

    juce::Slider timeSlider;
    juce::Slider feedbackSlider;
    juce::Slider blurSlider;
    juce::Slider dampingSlider;
    juce::Slider widthSlider;
    juce::Slider mixSlider;

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::vector<std::unique_ptr<SliderAttachment>> attachments;

    void initSlider (juce::Slider&, const juce::String&);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MDLGhostEchoAudioProcessorEditor)
};
