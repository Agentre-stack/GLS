#pragma once

#include <JuceHeader.h>
#include "../../DualPrecisionAudioProcessor.h"
#include "../../ui/GoodluckLookAndFeel.h"

class GLSMonoizeProAudioProcessor : public DualPrecisionAudioProcessor
{
public:
    GLSMonoizeProAudioProcessor();
    ~GLSMonoizeProAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "GLSMonoizePro"; }
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
    juce::AudioProcessorValueTreeState apvts;
    double currentSampleRate = 44100.0;
    juce::uint32 lastBlockSize = 512;

    juce::dsp::IIR::Filter<float> monoLowFilter;
    juce::dsp::IIR::Filter<float> stereoHighFilter;
    juce::AudioBuffer<float> dryBuffer;

    void updateFilters (float monoFreq, float stereoFreq);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GLSMonoizeProAudioProcessor)
};

class MonoizeVisual;

class GLSMonoizeProAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit GLSMonoizeProAudioProcessorEditor (GLSMonoizeProAudioProcessor&);
    ~GLSMonoizeProAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    GLSMonoizeProAudioProcessor& processorRef;

    juce::Colour accentColour;
    gls::ui::GoodluckLookAndFeel lookAndFeel;
    gls::ui::GoodluckHeader headerComponent;
    gls::ui::GoodluckFooter footerComponent;
    std::unique_ptr<MonoizeVisual> centerVisual;

    juce::Slider monoBelowSlider;
    juce::Slider stereoAboveSlider;
    juce::Slider widthSlider;
    juce::Slider centerLiftSlider;
    juce::Slider sideTrimSlider;
    juce::Slider inputTrimSlider;
    juce::Slider dryWetSlider;
    juce::Slider outputTrimSlider;
    juce::ToggleButton bypassButton { "Soft Bypass" };

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
    std::vector<std::unique_ptr<SliderAttachment>> sliderAttachments;
    std::vector<std::unique_ptr<ButtonAttachment>> buttonAttachments;

    struct LabeledSliderRef
    {
        juce::Slider* slider = nullptr;
        juce::Label* label = nullptr;
    };

    std::vector<std::unique_ptr<juce::Label>> sliderLabels;
    std::vector<LabeledSliderRef> labeledSliders;

    void configureSlider (juce::Slider& slider, const juce::String& targetName, bool isMacro, bool isLinear = false);
    void configureToggle (juce::ToggleButton& toggle);
    void layoutLabels();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GLSMonoizeProAudioProcessorEditor)
};
