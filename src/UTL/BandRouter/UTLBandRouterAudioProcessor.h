#pragma once

#include <JuceHeader.h>
#include <array>
#include <atomic>
#include "../../DualPrecisionAudioProcessor.h"
#include "../../ui/GoodluckLookAndFeel.h"

class UTLBandRouterAudioProcessor : public DualPrecisionAudioProcessor
{
public:
    UTLBandRouterAudioProcessor();
    ~UTLBandRouterAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "UTLBandRouter"; }
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
    float getBandMeter (int bandIndex) const noexcept;

private:
    juce::AudioProcessorValueTreeState apvts;
    juce::AudioBuffer<float> dryBuffer;
    std::array<juce::dsp::IIR::Filter<float>, 2> lowFilters;
    std::array<juce::dsp::IIR::Filter<float>, 2> highFilters;
    double currentSampleRate = 44100.0;
    float currentLowSplit = 150.0f;
    float currentHighSplit = 2500.0f;
    std::array<std::atomic<float>, 3> bandMeters { { 0.0f, 0.0f, 0.0f } };
    int currentPreset = 0;

    void updateFilters (float lowHz, float highHz);
    void applyPreset (int index);

    struct Preset
    {
        const char* name;
        std::vector<std::pair<const char*, float>> params;
    };

    static const std::array<Preset, 3> presetBank;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (UTLBandRouterAudioProcessor)
};

class UTLBandRouterAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit UTLBandRouterAudioProcessorEditor (UTLBandRouterAudioProcessor&);
    ~UTLBandRouterAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    UTLBandRouterAudioProcessor& processorRef;

    juce::Colour accentColour;
    gls::ui::GoodluckLookAndFeel lookAndFeel;
    gls::ui::GoodluckHeader headerComponent;
    gls::ui::GoodluckFooter footerComponent;
    std::unique_ptr<juce::Component> heroVisual;

    juce::Slider lowLevelSlider;
    juce::Slider midLevelSlider;
    juce::Slider highLevelSlider;
    juce::Slider lowSplitSlider;
    juce::Slider highSplitSlider;
    juce::Slider lowPanSlider;
    juce::Slider midPanSlider;
    juce::Slider highPanSlider;
    juce::Slider mixSlider;
    juce::Slider inputTrimSlider;
    juce::Slider outputTrimSlider;
    juce::ToggleButton soloLowButton { "Solo Low" };
    juce::ToggleButton soloMidButton { "Solo Mid" };
    juce::ToggleButton soloHighButton { "Solo High" };
    juce::ToggleButton bypassButton { "Soft Bypass" };

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
    std::vector<std::unique_ptr<SliderAttachment>> sliderAttachments;
    std::vector<std::unique_ptr<ButtonAttachment>> buttonAttachments;
    std::vector<std::unique_ptr<juce::Label>> labels;

    void configureRotarySlider (juce::Slider&, const juce::String&);
    void configureLinearSlider (juce::Slider&, const juce::String&, bool isHorizontal);
    void configureToggle (juce::ToggleButton&, const juce::String&);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (UTLBandRouterAudioProcessorEditor)
};
