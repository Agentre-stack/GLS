#pragma once

#include <JuceHeader.h>
#include <array>
#include "../../DualPrecisionAudioProcessor.h"
#include "../../ui/GoodluckLookAndFeel.h"

class EQFormSetAudioProcessor : public DualPrecisionAudioProcessor
{
public:
    EQFormSetAudioProcessor();
    ~EQFormSetAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "EQFormSet"; }
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
    struct FormantFilter
    {
        juce::dsp::IIR::Filter<float> filter;
        float phase = 0.0f;
    };

    std::vector<FormantFilter> formantFilters;
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
    void updateFormantFilters (float baseFreq, float width, float movement);
    void applyPreset (int index);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EQFormSetAudioProcessor)
};

class EQFormSetAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit EQFormSetAudioProcessorEditor (EQFormSetAudioProcessor&);
    ~EQFormSetAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    EQFormSetAudioProcessor& processorRef;

    juce::Colour accentColour;
    gls::ui::GoodluckLookAndFeel lookAndFeel;
    gls::ui::GoodluckHeader headerComponent;
    gls::ui::GoodluckFooter footerComponent;

    juce::Slider formantFreqSlider;
    juce::Slider formantWidthSlider;
    juce::Slider movementSlider;
    juce::Slider intensitySlider;
    juce::Slider mixSlider;
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

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EQFormSetAudioProcessorEditor)
};
