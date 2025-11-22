#pragma once

#include <JuceHeader.h>
#include <array>
#include "../../DualPrecisionAudioProcessor.h"
#include "../../ui/GoodluckLookAndFeel.h"

class GRDWarmLiftAudioProcessor : public DualPrecisionAudioProcessor
{
public:
    GRDWarmLiftAudioProcessor();
    ~GRDWarmLiftAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "GRDWarmLift"; }
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
    struct ChannelState
    {
        juce::dsp::IIR::Filter<float> warmthShelf;
        juce::dsp::IIR::Filter<float> shineShelf;
        juce::dsp::IIR::Filter<float> tightenFilter;
    };

    juce::AudioProcessorValueTreeState apvts;
    std::vector<ChannelState> channelState;
    double currentSampleRate = 44100.0;
    juce::uint32 lastBlockSize = 512;
    double filterSpecSampleRate = 0.0;
    juce::uint32 filterSpecBlockSize = 0;
    int currentPreset = 0;

    struct Preset
    {
        const char* name;
        std::vector<std::pair<const char*, float>> params;
    };

    static const std::array<Preset, 3> presetBank;

    void ensureStateSize (int numChannels);
    void updateFilters (float warmth, float shine, float tighten);
    void applyPreset (int index);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GRDWarmLiftAudioProcessor)
};

class GRDWarmLiftAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit GRDWarmLiftAudioProcessorEditor (GRDWarmLiftAudioProcessor&);
    ~GRDWarmLiftAudioProcessorEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    GRDWarmLiftAudioProcessor& processorRef;

    juce::Colour accentColour;
    gls::ui::GoodluckLookAndFeel lookAndFeel;
    gls::ui::GoodluckHeader headerComponent;
    gls::ui::GoodluckFooter footerComponent;

    juce::Slider warmthSlider;
    juce::Slider shineSlider;
    juce::Slider driveSlider;
    juce::Slider tightenSlider;
    juce::Slider mixSlider;
    juce::Slider inputTrimSlider;
    juce::Slider outputTrimSlider;
    juce::ToggleButton bypassButton { "Soft Bypass" };

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::vector<std::unique_ptr<SliderAttachment>> attachments;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttachment;
    std::vector<std::unique_ptr<juce::Label>> labels;

    void initSlider (juce::Slider&, const juce::String&, bool macro = false);
    void initToggle (juce::ToggleButton& toggle);
    void layoutLabels();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GRDWarmLiftAudioProcessorEditor)
};
