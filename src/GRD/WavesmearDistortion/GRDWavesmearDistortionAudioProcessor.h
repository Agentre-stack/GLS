#pragma once

#include <JuceHeader.h>

class GRDWavesmearDistortionAudioProcessor : public juce::AudioProcessor
{
public:
    GRDWavesmearDistortionAudioProcessor();
    ~GRDWavesmearDistortionAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "GRDWavesmearDistortion"; }
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
    std::vector<juce::dsp::IIR::Filter<float>> preFilters;
    std::vector<juce::dsp::IIR::Filter<float>> toneFilters;
    std::vector<float> smearMemory;
    juce::AudioBuffer<float> dryBuffer;
    juce::uint32 lastBlockSize = 0;
    double currentSampleRate = 44100.0;

    void ensureStateSize (int numChannels, int numSamples);
    void updateFilters (float preFreq, float toneFreq);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GRDWavesmearDistortionAudioProcessor)
};

class GRDWavesmearDistortionAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit GRDWavesmearDistortionAudioProcessorEditor (GRDWavesmearDistortionAudioProcessor&);
    ~GRDWavesmearDistortionAudioProcessorEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    GRDWavesmearDistortionAudioProcessor& processorRef;

    juce::Slider preFilterSlider;
    juce::Slider smearSlider;
    juce::Slider driveSlider;
    juce::Slider toneSlider;
    juce::Slider mixSlider;

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::vector<std::unique_ptr<SliderAttachment>> attachments;

    void initSlider (juce::Slider& slider, const juce::String& label);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GRDWavesmearDistortionAudioProcessorEditor)
};
