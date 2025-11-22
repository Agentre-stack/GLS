#pragma once

#include <JuceHeader.h>
#include <array>
#include "../../DualPrecisionAudioProcessor.h"
#include "../../ui/GoodluckLookAndFeel.h"

class GLSChannelPilotAudioProcessor : public DualPrecisionAudioProcessor
{
public:
    GLSChannelPilotAudioProcessor();
    ~GLSChannelPilotAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "GLSChannelPilot"; }
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
    float getAutoGainMeter() const noexcept { return lastAutoGain.load(); }

private:
    juce::AudioProcessorValueTreeState apvts;
    double currentSampleRate = 44100.0;
    float autoGainState = 1.0f;
    std::atomic<float> lastAutoGain { 1.0f };
    int currentPreset = 0;

    struct FilterPair
    {
        std::array<juce::dsp::IIR::Filter<float>, 2> highPass;
        std::array<juce::dsp::IIR::Filter<float>, 2> lowPass;
    };

    std::vector<FilterPair> filterPairs;
    struct Preset
    {
        const char* name;
        std::vector<std::pair<const char*, float>> params;
    };
    static const std::array<Preset, 3> presetBank;

    void updateFilterCoefficients (float hpfFreq, float lpfFreq, int slopeChoice);
    void ensureFilterStateSize();
    void applyPreset (int index);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GLSChannelPilotAudioProcessor)
};

class GLSChannelPilotAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit GLSChannelPilotAudioProcessorEditor (GLSChannelPilotAudioProcessor&);
    ~GLSChannelPilotAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    GLSChannelPilotAudioProcessor& processorRef;
    gls::ui::GoodluckLookAndFeel lookAndFeel;
    juce::Colour accentColour;
    gls::ui::GoodluckHeader headerComponent;
    gls::ui::GoodluckFooter footerComponent;

    std::unique_ptr<juce::Component> heroComponent;
    juce::Slider hpfSlider;
    juce::Slider lpfSlider;
    juce::Slider panSlider;
    juce::Slider inputTrimSlider;
    juce::Slider outputTrimSlider;
    juce::ComboBox filterSlopeBox;
    juce::ToggleButton phaseButton { "Phase" };
    juce::ToggleButton autoGainButton { "Auto Gain" };
    juce::ToggleButton bypassButton { "Soft Bypass" };

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using ComboBoxAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    std::unique_ptr<SliderAttachment> hpfAttachment;
    std::unique_ptr<SliderAttachment> lpfAttachment;
    std::unique_ptr<SliderAttachment> panAttachment;
    std::unique_ptr<SliderAttachment> inputTrimAttachment;
    std::unique_ptr<SliderAttachment> outputTrimAttachment;
    std::unique_ptr<ComboBoxAttachment> slopeAttachment;
    std::unique_ptr<ButtonAttachment> phaseAttachment;
    std::unique_ptr<ButtonAttachment> autoGainAttachment;
    std::unique_ptr<ButtonAttachment> bypassAttachment;

    void configureSlider (juce::Slider&, const juce::String&);
    void configureToggle (juce::ToggleButton&, const juce::String&);
    void configureComboBox (juce::ComboBox&);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GLSChannelPilotAudioProcessorEditor)
};
