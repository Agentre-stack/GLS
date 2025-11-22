#pragma once

#include <JuceHeader.h>
#include "../../DualPrecisionAudioProcessor.h"

class PITTimeStackAudioProcessor : public DualPrecisionAudioProcessor
{
public:
    PITTimeStackAudioProcessor();
    ~PITTimeStackAudioProcessor() override = default;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    //==============================================================================
    const juce::String getName() const override { return "PITTimeStack"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    //==============================================================================
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int index) override
    {
        return index == 0 ? juce::String ("PIT Time Stack 01") : juce::String();
    }
    void changeProgramName (int, const juce::String&) override {}

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getValueTreeState() { return apvts; }
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

private:
    juce::AudioProcessorValueTreeState apvts;
    juce::AudioBuffer<float> dryBuffer;
    double currentSampleRate = 44100.0;
    juce::AudioBuffer<float> monoBuffer;
    juce::AudioBuffer<float> tapScratchBuffer;
    juce::AudioBuffer<float> wetBuffer;

    static constexpr size_t kNumTaps = 4;
    using DelayLine = juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear>;
    std::array<DelayLine, kNumTaps> tapDelayLines;
    juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>,
                                   juce::dsp::IIR::Coefficients<float>> hpfProcessor;
    juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>,
                                   juce::dsp::IIR::Coefficients<float>> lpfProcessor;
    float lastHpfCutoff = 120.0f;
    float lastLpfCutoff = 15000.0f;

    void updateFilters (float hpf, float lpf);
    void ensureBuffers (int numChannels, int numSamples);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PITTimeStackAudioProcessor)
};

//==============================================================================
class PITTimeStackAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit PITTimeStackAudioProcessorEditor (PITTimeStackAudioProcessor&);
    ~PITTimeStackAudioProcessorEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    PITTimeStackAudioProcessor& processorRef;

    std::array<juce::Slider, 4> tapTimeSliders;
    std::array<juce::Slider, 4> tapLevelSliders;
    std::array<juce::Slider, 4> tapPanSliders;
    juce::Slider hpfSlider;
    juce::Slider lpfSlider;
    juce::Slider swingSlider;
    juce::Slider mixSlider;

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::vector<std::unique_ptr<SliderAttachment>> sliderAttachments;

    void initSlider (juce::Slider& slider, const juce::String& labelText);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PITTimeStackAudioProcessorEditor)
};
