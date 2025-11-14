#pragma once

#include <JuceHeader.h>

class GLSMonoizeProAudioProcessor : public juce::AudioProcessor
{
public:
    GLSMonoizeProAudioProcessor();
    ~GLSMonoizeProAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "GLSMonoizePro"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getValueTreeState() { return apvts; }
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

private:
    juce::AudioProcessorValueTreeState apvts;
    double currentSampleRate = 44100.0;

    juce::dsp::IIR::Filter<float> monoLowFilter;
    juce::dsp::IIR::Filter<float> stereoHighFilter;

    void updateFilters (float monoFreq, float stereoFreq);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GLSMonoizeProAudioProcessor)
};

class GLSMonoizeProAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit GLSMonoizeProAudioProcessorEditor (GLSMonoizeProAudioProcessor&);
    ~GLSMonoizeProAudioProcessorEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    GLSMonoizeProAudioProcessor& processorRef;

    juce::Slider monoBelowSlider;
    juce::Slider stereoAboveSlider;
    juce::Slider widthSlider;
    juce::Slider centerLiftSlider;
    juce::Slider sideTrimSlider;

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::vector<std::unique_ptr<SliderAttachment>> attachments;

    void initialiseSlider (juce::Slider& slider, const juce::String& targetName);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GLSMonoizeProAudioProcessorEditor)
};
