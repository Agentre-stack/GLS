#pragma once

#include <JuceHeader.h>
#include "../../DualPrecisionAudioProcessor.h"

class PITMicroShiftAudioProcessor : public DualPrecisionAudioProcessor
{
public:
    PITMicroShiftAudioProcessor();
    ~PITMicroShiftAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "PITMicroShift"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int index) override
    {
        return index == 0 ? juce::String ("PIT Micro Shift 01") : juce::String();
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
    double currentSampleRate = 44100.0;
    std::array<juce::dsp::Chorus<float>, 2> chorusProcessors;
    juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>,
                                   juce::dsp::IIR::Coefficients<float>> hpfProcessor;
    float lastHpfCutoff = 120.0f;

    void updateHighPass (float cutoffHz);
    void processStereoWidth (float widthValue, int numSamples);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PITMicroShiftAudioProcessor)
};

//==============================================================================
class PITMicroShiftAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit PITMicroShiftAudioProcessorEditor (PITMicroShiftAudioProcessor&);
    ~PITMicroShiftAudioProcessorEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    PITMicroShiftAudioProcessor& processorRef;

    juce::Slider detuneLSlider;
    juce::Slider detuneRSlider;
    juce::Slider delayLSlider;
    juce::Slider delayRSlider;
    juce::Slider widthSlider;
    juce::Slider hpfSlider;
    juce::Slider mixSlider;

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::vector<std::unique_ptr<SliderAttachment>> attachments;

    void initSlider (juce::Slider& slider, const juce::String& label);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PITMicroShiftAudioProcessorEditor)
};
