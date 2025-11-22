#pragma once

#include <JuceHeader.h>
#include <array>
#include "../../DualPrecisionAudioProcessor.h"
#include "../../ui/GoodluckLookAndFeel.h"

class EQDynamicTiltProAudioProcessor : public DualPrecisionAudioProcessor
{
public:
    EQDynamicTiltProAudioProcessor();
    ~EQDynamicTiltProAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "EQDynamicTiltPro"; }
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

    float getCurrentTiltDb() const noexcept { return currentTilt.load(); }
    float getEnvelopeDb() const noexcept    { return lastEnvelopeDb.load(); }
    float getThresholdDb() const noexcept   { return lastThresholdDb.load(); }

private:
    juce::AudioProcessorValueTreeState apvts;
    std::vector<juce::dsp::IIR::Filter<float>> lowShelves;
    std::vector<juce::dsp::IIR::Filter<float>> highShelves;
    std::vector<float> envelopes;
    double currentSampleRate = 44100.0;
    juce::uint32 lastBlockSize = 512;
    juce::AudioBuffer<float> dryBuffer;
    std::atomic<float> currentTilt { 0.0f };
    std::atomic<float> lastEnvelopeDb { -120.0f };
    std::atomic<float> lastThresholdDb { -24.0f };
    int currentPreset = 0;

    struct Preset
    {
        const char* name;
        std::vector<std::pair<const char*, float>> params;
    };

    static const std::array<Preset, 3> presetBank;

    void ensureStateSize (int numChannels);
    void updateFilters (float totalTiltDb, float pivotFreq, float shelfQ);
    void applyPreset (int index);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EQDynamicTiltProAudioProcessor)
};

class TiltVisualComponent;

class EQDynamicTiltProAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit EQDynamicTiltProAudioProcessorEditor (EQDynamicTiltProAudioProcessor&);
    ~EQDynamicTiltProAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    EQDynamicTiltProAudioProcessor& processorRef;

    juce::Colour accentColour;
    gls::ui::GoodluckLookAndFeel lookAndFeel;
    gls::ui::GoodluckHeader headerComponent;
    gls::ui::GoodluckFooter footerComponent;
    std::unique_ptr<TiltVisualComponent> centerVisual;

    juce::Slider tiltSlider;
    juce::Slider pivotSlider;
    juce::Slider threshSlider;
    juce::Slider rangeSlider;
    juce::Slider attackSlider;
    juce::Slider releaseSlider;
    juce::Slider mixSlider;
    juce::Slider inputTrimSlider;
    juce::Slider outputTrimSlider;
    juce::ComboBox detectorModeBox;
    juce::ComboBox styleBox;
    juce::ToggleButton bypassButton { "Soft Bypass" };

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using ComboAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    std::vector<std::unique_ptr<SliderAttachment>> sliderAttachments;
    std::vector<std::unique_ptr<ButtonAttachment>> buttonAttachments;
    std::vector<std::unique_ptr<ComboAttachment>> comboAttachments;

    struct LabeledSliderRef
    {
        juce::Slider* slider = nullptr;
        juce::Label* label = nullptr;
    };

    std::vector<std::unique_ptr<juce::Label>> sliderLabels;
    std::vector<LabeledSliderRef> labeledSliders;

    void configureSlider (juce::Slider& slider, const juce::String& label, bool isMacro, bool isLinear = false);
    void configureToggle (juce::ToggleButton& toggle);
    void layoutLabels();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EQDynamicTiltProAudioProcessorEditor)
};
