#pragma once

#include <JuceHeader.h>
#include <array>
#include "../../DualPrecisionAudioProcessor.h"
#include "../../ui/GoodluckLookAndFeel.h"

class EQAirGlassAudioProcessor : public DualPrecisionAudioProcessor
{
public:
    EQAirGlassAudioProcessor();
    ~EQAirGlassAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "EQAirGlass"; }
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
    std::vector<juce::dsp::IIR::Filter<float>> airShelves;
    std::vector<juce::dsp::IIR::Filter<float>> harshFilters;
    std::vector<float> harshEnvelopes;
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
    void updateShelfCoefficients (float freq, float gainDb);
    void updateHarshFilters (float freq);
    void applyPreset (int index);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EQAirGlassAudioProcessor)
};

class EQAirGlassAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit EQAirGlassAudioProcessorEditor (EQAirGlassAudioProcessor&);
    ~EQAirGlassAudioProcessorEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    EQAirGlassAudioProcessor& processorRef;

    juce::Colour accentColour;
    gls::ui::GoodluckLookAndFeel lookAndFeel;
    gls::ui::GoodluckHeader headerComponent;
    gls::ui::GoodluckFooter footerComponent;

    juce::Slider airFreqSlider;
    juce::Slider airGainSlider;
    juce::Slider harmonicBlendSlider;
    juce::Slider deHarshSlider;
    juce::Slider inputTrimSlider;
    juce::Slider outputTrimSlider;
    juce::ToggleButton bypassButton { "Soft Bypass" };

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
    std::vector<std::unique_ptr<SliderAttachment>> attachments;
    std::vector<std::unique_ptr<ButtonAttachment>> buttonAttachments;
    std::vector<std::unique_ptr<juce::Label>> labels;

    void initSlider (juce::Slider& slider, const juce::String& label, bool macro = false);
    void initToggle (juce::ToggleButton& toggle);
    void layoutLabels();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EQAirGlassAudioProcessorEditor)
};
