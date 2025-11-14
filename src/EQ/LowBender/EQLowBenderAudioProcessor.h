#pragma once

#include <JuceHeader.h>

class EQLowBenderAudioProcessor : public juce::AudioProcessor
{
public:
    EQLowBenderAudioProcessor();
    ~EQLowBenderAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "EQLowBender"; }
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
    std::vector<juce::dsp::IIR::Filter<float>> subShelves;
    std::vector<juce::dsp::IIR::Filter<float>> punchFilters;
    std::vector<juce::dsp::IIR::Filter<float>> lowCuts;
    double currentSampleRate = 44100.0;
    juce::uint32 lastBlockSize = 512;

    void ensureFilterState (int numChannels);
    void updateFilters (float subBoostDb, float punchFreq, float punchGainDb,
                        float lowCutFreq, float tightness);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EQLowBenderAudioProcessor)
};

class EQLowBenderAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit EQLowBenderAudioProcessorEditor (EQLowBenderAudioProcessor&);
    ~EQLowBenderAudioProcessorEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    EQLowBenderAudioProcessor& processorRef;

    juce::Slider subBoostSlider;
    juce::Slider lowCutSlider;
    juce::Slider punchFreqSlider;
    juce::Slider punchGainSlider;
    juce::Slider tightnessSlider;

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::vector<std::unique_ptr<SliderAttachment>> attachments;

    void initSlider (juce::Slider& slider, const juce::String& label);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EQLowBenderAudioProcessorEditor)
};
