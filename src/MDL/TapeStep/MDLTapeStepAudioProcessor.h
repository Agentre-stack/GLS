#pragma once

#include <JuceHeader.h>
#include "../../DualPrecisionAudioProcessor.h"

class MDLTapeStepAudioProcessor : public DualPrecisionAudioProcessor
{
public:
    MDLTapeStepAudioProcessor();
    ~MDLTapeStepAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "MDLTapeStep"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 2.0; }

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

    struct TapeLine
    {
        juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> delay { 192000 };
        juce::dsp::IIR::Filter<float> toneFilter;
        float wowPhase = 0.0f;
        float flutterPhase = 0.0f;
        float feedbackSample = 0.0f;
    };

    std::vector<TapeLine> tapeLines;
    juce::AudioBuffer<float> dryBuffer;

    double currentSampleRate = 44100.0;
    juce::uint32 lastBlockSize = 512;
    double lineSpecSampleRate = 0.0;
    juce::uint32 lineSpecBlockSize = 0;
    juce::Random random;

    void ensureStateSize (int numChannels);
    void updateToneFilters (float tone);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MDLTapeStepAudioProcessor)
};

class MDLTapeStepAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit MDLTapeStepAudioProcessorEditor (MDLTapeStepAudioProcessor&);
    ~MDLTapeStepAudioProcessorEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    MDLTapeStepAudioProcessor& processorRef;

    juce::Slider timeSlider;
    juce::Slider feedbackSlider;
    juce::Slider driveSlider;
    juce::Slider wowSlider;
    juce::Slider flutterSlider;
    juce::Slider toneSlider;
    juce::Slider mixSlider;

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::vector<std::unique_ptr<SliderAttachment>> attachments;

    void initSlider (juce::Slider&, const juce::String&);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MDLTapeStepAudioProcessorEditor)
};
