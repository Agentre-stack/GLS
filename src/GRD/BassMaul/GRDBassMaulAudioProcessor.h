#pragma once

#include <JuceHeader.h>
#include "../../DualPrecisionAudioProcessor.h"
#include "../../ui/GoodluckLookAndFeel.h"

class GRDBassMaulAudioProcessor : public DualPrecisionAudioProcessor
{
public:
    GRDBassMaulAudioProcessor();
    ~GRDBassMaulAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "GRDBassMaul"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int index) override { return index == 0 ? juce::String (JucePlugin_Name " 01") : juce::String(); }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getValueTreeState() { return apvts; }
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

private:
    struct ChannelState
    {
        juce::dsp::IIR::Filter<float> tightHighpass;
        juce::dsp::IIR::Filter<float> subLowpass;
    };

    juce::AudioProcessorValueTreeState apvts;
    std::vector<ChannelState> channelStates;
    double currentSampleRate = 44100.0;
    juce::uint32 lastBlockSize = 512;
    double filterSpecSampleRate = 0.0;
    juce::uint32 filterSpecBlockSize = 0;

    void ensureChannelState (int numChannels);
    void updateFilterCoefficients (float tightnessHz, float subSplitHz);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GRDBassMaulAudioProcessor)
};

class BassMaulVisual;

class GRDBassMaulAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit GRDBassMaulAudioProcessorEditor (GRDBassMaulAudioProcessor&);
    ~GRDBassMaulAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    GRDBassMaulAudioProcessor& processorRef;

    juce::Colour accentColour;
    gls::ui::GoodluckLookAndFeel lookAndFeel;
    gls::ui::GoodluckHeader headerComponent;
    gls::ui::GoodluckFooter footerComponent;
    std::unique_ptr<BassMaulVisual> centerVisual;

    juce::Slider driveSlider;
    juce::Slider subBoostSlider;
    juce::Slider tightnessSlider;
    juce::Slider blendSlider;
    juce::Slider trimSlider;
    juce::Slider inputTrimSlider;

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

    void configureSlider (juce::Slider&, const juce::String&, bool isMacro, bool isLinear = false);
    void layoutLabels();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GRDBassMaulAudioProcessorEditor)
};
