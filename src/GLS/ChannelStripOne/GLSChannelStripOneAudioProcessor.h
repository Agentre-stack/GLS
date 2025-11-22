#pragma once

#include <JuceHeader.h>
#include "../../DualPrecisionAudioProcessor.h"
#include "../../ui/GoodluckLookAndFeel.h"

class GLSChannelStripOneAudioProcessor : public DualPrecisionAudioProcessor
{
public:
    GLSChannelStripOneAudioProcessor();
    ~GLSChannelStripOneAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "GLSChannelStripOne"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int index) override
    {
        return index == 0 ? juce::String ("GLS Channel Strip One 01") : juce::String();
    }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getValueTreeState() { return apvts; }

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

private:
    juce::AudioProcessorValueTreeState apvts;
    struct ChannelState
    {
        juce::dsp::IIR::Filter<float> lowShelf;
        juce::dsp::IIR::Filter<float> lowMidBell;
        juce::dsp::IIR::Filter<float> highMidBell;
        juce::dsp::IIR::Filter<float> highShelf;
        float gateEnvelope = 0.0f;
        float gateGain = 1.0f;
        float compEnvelope = 0.0f;
        float compGain = 1.0f;
    };

    std::vector<ChannelState> channelStates;
    juce::AudioBuffer<float> dryBuffer;
    double currentSampleRate = 44100.0;

    void ensureStateSize();
    void updateEqCoefficients (ChannelState& state,
                               float lowGain, float lowMidGain,
                               float highMidGain, float highGain);
    static float softClip (float input, float amount);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GLSChannelStripOneAudioProcessor)
};

class ChannelStripVisual;

class GLSChannelStripOneAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit GLSChannelStripOneAudioProcessorEditor (GLSChannelStripOneAudioProcessor&);
    ~GLSChannelStripOneAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    GLSChannelStripOneAudioProcessor& processorRef;

    juce::Colour accentColour;
    gls::ui::GoodluckLookAndFeel lookAndFeel;
    gls::ui::GoodluckHeader headerComponent;
    gls::ui::GoodluckFooter footerComponent;
    std::unique_ptr<ChannelStripVisual> centerVisual;

    juce::Slider gateThreshSlider;
    juce::Slider gateRangeSlider;
    juce::Slider compThreshSlider;
    juce::Slider compRatioSlider;
    juce::Slider compAttackSlider;
    juce::Slider compReleaseSlider;
    juce::Slider lowGainSlider;
    juce::Slider lowMidGainSlider;
    juce::Slider highMidGainSlider;
    juce::Slider highGainSlider;
    juce::Slider satAmountSlider;
    juce::Slider dryWetSlider;
    juce::Slider inputTrimSlider;
    juce::Slider outputTrimSlider;

    juce::ToggleButton bypassButton;

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
    std::vector<std::unique_ptr<SliderAttachment>> attachments;
    std::vector<std::unique_ptr<ButtonAttachment>> buttonAttachments;

    struct LabeledSliderRef
    {
        juce::Slider* slider = nullptr;
        juce::Label* label = nullptr;
    };

    std::vector<std::unique_ptr<juce::Label>> sliderLabels;
    std::vector<LabeledSliderRef> labeledSliders;

    void configureSlider (juce::Slider& slider, const juce::String& name, bool isMacro, bool isLinear = false);
    void layoutLabels();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GLSChannelStripOneAudioProcessorEditor)
};
