#pragma once

#include <JuceHeader.h>
#include <array>
#include "../../DualPrecisionAudioProcessor.h"

class EQHarmonicEQAudioProcessor : public DualPrecisionAudioProcessor
{
public:
    EQHarmonicEQAudioProcessor();
    ~EQHarmonicEQAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "EQHarmonicEQ"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentPreset; }
    void setCurrentProgram (int) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getValueTreeState() { return apvts; }
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

private:
    juce::AudioProcessorValueTreeState apvts;
    struct HarmonicState
    {
        juce::dsp::IIR::Filter<float> base;
        juce::dsp::IIR::Filter<float> harmonic;
    };

    std::vector<HarmonicState> harmonicBands;
    juce::AudioBuffer<float> dryBuffer;
    double currentSampleRate = 44100.0;
    juce::uint32 lastBlockSize = 512;
    int currentPreset = 0;

    struct Preset
    {
        const char* name;
        std::vector<std::pair<const char*, float>> params;
    };

    static const std::array<Preset, 3> presetBank;

    void ensureStateSize (int numChannels);
    void updateFilters (float freq, float q, float gainDb, int harmType);
    void applyPreset (int index);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EQHarmonicEQAudioProcessor)
};

class EQHarmonicEQAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit EQHarmonicEQAudioProcessorEditor (EQHarmonicEQAudioProcessor&);
    ~EQHarmonicEQAudioProcessorEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    EQHarmonicEQAudioProcessor& processorRef;

    juce::Slider bandFreqSlider;
    juce::Slider bandGainSlider;
    juce::Slider bandQSlider;
    juce::ComboBox harmTypeBox;
    juce::Slider mixSlider;

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboBoxAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    std::unique_ptr<SliderAttachment> bandFreqAttachment;
    std::unique_ptr<SliderAttachment> bandGainAttachment;
    std::unique_ptr<SliderAttachment> bandQAttachment;
    std::unique_ptr<ComboBoxAttachment> harmTypeAttachment;
    std::unique_ptr<SliderAttachment> mixAttachment;

    void initSlider (juce::Slider& slider, const juce::String& label);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EQHarmonicEQAudioProcessorEditor)
};
