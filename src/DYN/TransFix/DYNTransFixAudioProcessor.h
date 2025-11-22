#pragma once

#include <JuceHeader.h>
#include "../../DualPrecisionAudioProcessor.h"
#include "../../ui/GoodluckLookAndFeel.h"
#include <array>
#include <vector>

class DYNTransFixAudioProcessor : public DualPrecisionAudioProcessor
{
public:
    DYNTransFixAudioProcessor();
    ~DYNTransFixAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "DYNTransFix"; }
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
    struct ChannelState
    {
        float detector = 0.0f;
        float attackEnv = 0.0f;
        float sustainEnv = 0.0f;
        juce::dsp::IIR::Filter<float> hfFilter;
        juce::dsp::IIR::Filter<float> lfFilter;
    };

    std::vector<ChannelState> channelStates;
    double currentSampleRate = 44100.0;
    juce::uint32 lastBlockSize = 512;
    juce::AudioBuffer<float> dryBuffer;
    int currentPreset = 0;

    void ensureStateSize();
    float applyTilt (float sample, float freq, float amount);
    void applyPreset (int index);

    struct Preset
    {
        const char* name;
        std::vector<std::pair<const char*, float>> params;
    };

    static const std::array<Preset, 3> presetBank;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DYNTransFixAudioProcessor)
};

class DYNTransFixAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit DYNTransFixAudioProcessorEditor (DYNTransFixAudioProcessor&);
    ~DYNTransFixAudioProcessorEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    DYNTransFixAudioProcessor& processorRef;

    juce::Colour accentColour;
    gls::ui::GoodluckLookAndFeel lookAndFeel;
    gls::ui::GoodluckHeader headerComponent;
    gls::ui::GoodluckFooter footerComponent;

    juce::Slider attackSlider;
    juce::Slider sustainSlider;
    juce::Slider tiltFreqSlider;
    juce::Slider tiltAmountSlider;
    juce::ComboBox detectModeBox;
    juce::Slider mixSlider;
    juce::Slider inputTrimSlider;
    juce::Slider outputTrimSlider;
    juce::ToggleButton bypassButton { "Soft Bypass" };

    using SliderAttachment   = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboBoxAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using ButtonAttachment   = juce::AudioProcessorValueTreeState::ButtonAttachment;

    std::vector<std::unique_ptr<SliderAttachment>> sliderAttachments;
    std::unique_ptr<ComboBoxAttachment> detectModeAttachment;
    std::unique_ptr<ButtonAttachment> buttonAttachment;

    std::vector<std::unique_ptr<juce::Label>> labels;

    void initSlider (juce::Slider& slider, const juce::String& label, bool macro = false);
    void initToggle (juce::ToggleButton& toggle);
    void layoutLabels();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DYNTransFixAudioProcessorEditor)
};
