#pragma once

#include <JuceHeader.h>
#include <array>
#include "../../DualPrecisionAudioProcessor.h"
#include "../../ui/GoodluckLookAndFeel.h"

class DYNPunchGateAudioProcessor : public DualPrecisionAudioProcessor
{
public:
    DYNPunchGateAudioProcessor();
    ~DYNPunchGateAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "DYNPunchGate"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentPreset; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getValueTreeState() { return apvts; }
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    float getGateMeter() const noexcept { return gateMeter.load(); }

private:
    juce::AudioProcessorValueTreeState apvts;
    juce::AudioBuffer<float> dryBuffer;
    struct ChannelState
    {
        float envelope = 0.0f;
        float holdCounter = 0.0f;
        float gateGain = 1.0f;
    };

    std::vector<ChannelState> channelStates;
    double currentSampleRate = 44100.0;
    std::vector<juce::dsp::IIR::Filter<float>> scHighPassFilters;
    std::vector<juce::dsp::IIR::Filter<float>> scLowPassFilters;
    std::atomic<float> gateMeter { 0.0f };
    int currentPreset = 0;

    struct Preset
    {
        const char* name;
        std::vector<std::pair<const char*, float>> params;
    };

    static const std::array<Preset, 3> presetBank;

    void ensureStateSize();
    void applyPreset (int index);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DYNPunchGateAudioProcessor)
};

class DYNPunchGateAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit DYNPunchGateAudioProcessorEditor (DYNPunchGateAudioProcessor&);
    ~DYNPunchGateAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    DYNPunchGateAudioProcessor& processorRef;

    juce::Colour accentColour;
    gls::ui::GoodluckLookAndFeel lookAndFeel;
    gls::ui::GoodluckHeader headerComponent;
    gls::ui::GoodluckFooter footerComponent;
    std::unique_ptr<juce::Component> gateVisual;

    juce::Slider threshSlider;
    juce::Slider rangeSlider;
    juce::Slider attackSlider;
    juce::Slider holdSlider;
    juce::Slider releaseSlider;
    juce::Slider hysteresisSlider;
    juce::Slider punchBoostSlider;
    juce::Slider sidechainHpfSlider;
    juce::Slider sidechainLpfSlider;
    juce::Slider inputTrimSlider;
    juce::Slider mixSlider;
    juce::Slider outputTrimSlider;
    juce::ToggleButton bypassButton { "Soft Bypass" };

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
    std::vector<std::unique_ptr<SliderAttachment>> attachments;
    std::vector<std::unique_ptr<ButtonAttachment>> buttonAttachments;

    struct LabeledSlider
    {
        juce::Slider* slider = nullptr;
        std::unique_ptr<juce::Label> label;
    };

    std::vector<LabeledSlider> labeledSliders;

    void initialiseSlider (juce::Slider& slider, const juce::String& label);
    void initialiseLinearSlider (juce::Slider& slider, const juce::String& label);
    void configureToggle (juce::ToggleButton& toggle);
    void layoutLabels();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DYNPunchGateAudioProcessorEditor)
};
