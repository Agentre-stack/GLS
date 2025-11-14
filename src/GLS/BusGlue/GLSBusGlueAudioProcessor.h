#pragma once

#include <JuceHeader.h>

class GLSBusGlueAudioProcessor : public juce::AudioProcessor
{
public:
    GLSBusGlueAudioProcessor();
    ~GLSBusGlueAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "GLSBusGlue"; }
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
    juce::AudioBuffer<float> dryBuffer;
    juce::dsp::IIR::Filter<float> sidechainFilter;
    float detectorEnvelope = 0.0f;
    float gainSmoothed = 1.0f;

    void updateSidechainFilter (float frequency);
    float computeGainDb (float inputLevelDb, float thresholdDb, float ratio, float kneeDb) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GLSBusGlueAudioProcessor)
};

class GLSBusGlueAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit GLSBusGlueAudioProcessorEditor (GLSBusGlueAudioProcessor&);
    ~GLSBusGlueAudioProcessorEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    GLSBusGlueAudioProcessor& processorRef;

    juce::Slider threshSlider;
    juce::Slider ratioSlider;
    juce::Slider attackSlider;
    juce::Slider releaseSlider;
    juce::Slider kneeSlider;
    juce::Slider scHpfSlider;
    juce::Slider mixSlider;
    juce::Slider outputSlider;

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::vector<std::unique_ptr<SliderAttachment>> attachments;

    void initialiseSlider (juce::Slider& slider, const juce::String& name);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GLSBusGlueAudioProcessorEditor)
};
