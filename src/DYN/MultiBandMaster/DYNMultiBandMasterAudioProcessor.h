#pragma once

#include <JuceHeader.h>
#include <array>

class DYNMultiBandMasterAudioProcessor : public juce::AudioProcessor
{
public:
    DYNMultiBandMasterAudioProcessor();
    ~DYNMultiBandMasterAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "DYNMultiBandMaster"; }
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
    struct DynamicBand
    {
        juce::dsp::IIR::Filter<float> filter;
        float envelope = 0.0f;
        float gain = 1.0f;
    };

    std::array<std::vector<DynamicBand>, 3> bandStates;
    juce::AudioBuffer<float> dryBuffer;
    double currentSampleRate = 44100.0;
    juce::uint32 lastBlockSize = 512;

    void ensureBandStateSize (int numChannels);
    void updateBandFilters (const std::array<float, 3>& freqs);
    float computeCompressorGain (float levelDb, float threshDb, float ratio) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DYNMultiBandMasterAudioProcessor)
};

class DYNMultiBandMasterAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit DYNMultiBandMasterAudioProcessorEditor (DYNMultiBandMasterAudioProcessor&);
    ~DYNMultiBandMasterAudioProcessorEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    DYNMultiBandMasterAudioProcessor& processorRef;

    std::array<juce::Slider, 3> freqSliders;
    std::array<juce::Slider, 3> threshSliders;
    std::array<juce::Slider, 3> ratioSliders;
    juce::Slider mixSlider;
    juce::Slider outputTrimSlider;

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::vector<std::unique_ptr<SliderAttachment>> attachments;

    void initSlider (juce::Slider& slider, const juce::String& label);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DYNMultiBandMasterAudioProcessorEditor)
};
