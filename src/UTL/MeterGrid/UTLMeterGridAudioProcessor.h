#pragma once

#include <JuceHeader.h>
#include <array>
#include <atomic>
#include "../../DualPrecisionAudioProcessor.h"
#include "../../ui/GoodluckLookAndFeel.h"

class UTLMeterGridAudioProcessor : public DualPrecisionAudioProcessor
{
public:
    UTLMeterGridAudioProcessor();
    ~UTLMeterGridAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "UTLMeterGrid"; }
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

    struct MeterSnapshot
    {
        float rmsLeft  = 0.0f;
        float rmsRight = 0.0f;
        float peakLeft = 0.0f;
        float peakRight= 0.0f;
        float holdLeft = 0.0f;
        float holdRight= 0.0f;
        float crest    = 0.0f;
    };

    MeterSnapshot getMeterSnapshot() const noexcept;
    float getDisplayCeilingDb() const noexcept;
    int getScalePresetIndex() const noexcept;

private:
    juce::AudioProcessorValueTreeState apvts;

    std::array<float, 2> rmsState { 0.0f, 0.0f };
    std::array<float, 2> peakHoldValue { 0.0f, 0.0f };
    std::array<int, 2> peakHoldCountdown { 0, 0 };
    std::array<std::atomic<float>, 6> meterValues;
    std::atomic<float> crestValue { 0.0f };
    std::atomic<float> displayCeiling { 0.0f };
    std::atomic<int> scalePreset { 0 };
    double currentSampleRate = 44100.0;
    int currentPreset = 0;

    void updateScalePreset (int presetIndex);
    void applyPreset (int index);

    struct Preset
    {
        const char* name;
        std::vector<std::pair<const char*, float>> params;
    };

    static const std::array<Preset, 3> presetBank;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (UTLMeterGridAudioProcessor)
};

class UTLMeterGridAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit UTLMeterGridAudioProcessorEditor (UTLMeterGridAudioProcessor&);
    ~UTLMeterGridAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    UTLMeterGridAudioProcessor& processorRef;

    juce::Colour accentColour;
    gls::ui::GoodluckLookAndFeel lookAndFeel;
    gls::ui::GoodluckHeader headerComponent;
    gls::ui::GoodluckFooter footerComponent;
    std::unique_ptr<juce::Component> heroVisual;

    juce::Slider integrationSlider;
    juce::Slider peakHoldSlider;
    juce::Slider inputTrimSlider;
    juce::Slider outputTrimSlider;
    juce::ComboBox scaleSelector;
    juce::ToggleButton freezeButton { "Freeze" };
    juce::ToggleButton bypassButton { "Soft Bypass" };

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using ComboAttachment  = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    std::vector<std::unique_ptr<SliderAttachment>> sliderAttachments;
    std::vector<std::unique_ptr<ButtonAttachment>> buttonAttachments;
    std::unique_ptr<ComboAttachment> scaleAttachment;
    std::vector<std::unique_ptr<juce::Label>> labels;

    void configureRotarySlider (juce::Slider&, const juce::String&);
    void configureLinearSlider (juce::Slider&, const juce::String&);
    void configureToggle (juce::ToggleButton&, const juce::String&);
    void configureComboBox (juce::ComboBox&, const juce::String&);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (UTLMeterGridAudioProcessorEditor)
};
