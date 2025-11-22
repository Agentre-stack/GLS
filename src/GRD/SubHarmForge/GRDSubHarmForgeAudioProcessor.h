#pragma once

#include <JuceHeader.h>
#include "../../DualPrecisionAudioProcessor.h"

class GRDSubHarmForgeAudioProcessor : public DualPrecisionAudioProcessor
{
public:
    GRDSubHarmForgeAudioProcessor();
    ~GRDSubHarmForgeAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "GRDSubHarmForge"; }
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
    struct ChannelState
    {
        juce::dsp::IIR::Filter<float> lowFilter;
        juce::dsp::IIR::Filter<float> subFilter;
    };

    juce::AudioProcessorValueTreeState apvts;
    std::vector<ChannelState> channelState;
    double currentSampleRate = 44100.0;
    juce::uint32 lastBlockSize = 512;
    double filterSpecSampleRate = 0.0;
    juce::uint32 filterSpecBlockSize = 0;
    float subPhase = 0.0f;

    void ensureStateSize (int numChannels);
    void updateFilters (float crossoverHz);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GRDSubHarmForgeAudioProcessor)
};

class GRDSubHarmForgeAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit GRDSubHarmForgeAudioProcessorEditor (GRDSubHarmForgeAudioProcessor&);
    ~GRDSubHarmForgeAudioProcessorEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    GRDSubHarmForgeAudioProcessor& processorRef;

    juce::Slider depthSlider;
    juce::Slider crossoverSlider;
    juce::Slider driveSlider;
    juce::Slider blendSlider;
    juce::Slider trimSlider;

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::vector<std::unique_ptr<SliderAttachment>> attachments;

    void initSlider (juce::Slider&, const juce::String&);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GRDSubHarmForgeAudioProcessorEditor)
};
