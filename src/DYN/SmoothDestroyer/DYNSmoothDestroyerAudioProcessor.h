#pragma once

#include <JuceHeader.h>
#include "../../DualPrecisionAudioProcessor.h"
#include "../../ui/GoodluckLookAndFeel.h"
#include <array>
#include <vector>

class DYNSmoothDestroyerAudioProcessor : public DualPrecisionAudioProcessor
{
public:
    DYNSmoothDestroyerAudioProcessor();
    ~DYNSmoothDestroyerAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "DYNSmoothDestroyer"; }
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
    struct DynamicBand
    {
        juce::dsp::IIR::Filter<float> bandFilter;
        float envelope = 0.0f;
        float gain = 1.0f;
    };

    std::vector<DynamicBand> band1States;
    std::vector<DynamicBand> band2States;
    juce::AudioBuffer<float> dryBuffer;
    double currentSampleRate = 44100.0;
    juce::uint32 lastBlockSize = 512;
    int currentPreset = 0;

    void ensureStateSize();
    void updateBandCoefficients (DynamicBand& band, float freq, float q);
    float computeBandGain (float levelDb, float threshDb, float rangeDb) const;
    void applyPreset (int index);

    struct Preset
    {
        const char* name;
        std::vector<std::pair<const char*, float>> params;
    };

    static const std::array<Preset, 3> presetBank;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DYNSmoothDestroyerAudioProcessor)
};

class DYNSmoothDestroyerAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit DYNSmoothDestroyerAudioProcessorEditor (DYNSmoothDestroyerAudioProcessor&);
    ~DYNSmoothDestroyerAudioProcessorEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    DYNSmoothDestroyerAudioProcessor& processorRef;

    juce::Colour accentColour;
    gls::ui::GoodluckLookAndFeel lookAndFeel;
    gls::ui::GoodluckHeader headerComponent;
    gls::ui::GoodluckFooter footerComponent;

    juce::Slider band1FreqSlider;
    juce::Slider band1QSlider;
    juce::Slider band1ThreshSlider;
    juce::Slider band1RangeSlider;
    juce::Slider band2FreqSlider;
    juce::Slider band2QSlider;
    juce::Slider band2ThreshSlider;
    juce::Slider band2RangeSlider;
    juce::Slider globalAttackSlider;
    juce::Slider globalReleaseSlider;
    juce::Slider mixSlider;
    juce::Slider inputTrimSlider;
    juce::Slider outputTrimSlider;
    juce::ToggleButton bypassButton { "Soft Bypass" };

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
    std::vector<std::unique_ptr<SliderAttachment>> attachments;
    std::vector<std::unique_ptr<ButtonAttachment>> buttonAttachments;
    std::vector<std::unique_ptr<juce::Label>> labels;

    void initialiseSlider (juce::Slider& slider, const juce::String& label, bool macro = false);
    void initToggle (juce::ToggleButton& toggle);
    void layoutLabels();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DYNSmoothDestroyerAudioProcessorEditor)
};
