#pragma once

#include <JuceHeader.h>
#include "../../DualPrecisionAudioProcessor.h"
#include "../../ui/GoodluckLookAndFeel.h"
#include <array>

class DYNBusLiftAudioProcessor : public DualPrecisionAudioProcessor
{
public:
    DYNBusLiftAudioProcessor();
    ~DYNBusLiftAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "DYNBusLift"; }
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
    juce::AudioBuffer<float> dryBuffer;
    juce::AudioBuffer<float> lowBuffer;
    juce::AudioBuffer<float> midBuffer;
    juce::AudioBuffer<float> highBuffer;
    juce::dsp::LinkwitzRileyFilter<float> lowLowpass, lowHighpass;
    juce::dsp::LinkwitzRileyFilter<float> midLowpass, midHighpass;
    juce::dsp::LinkwitzRileyFilter<float> highLowpass, highHighpass;
    double currentSampleRate = 44100.0;
    int currentPreset = 0;

    void processBand (juce::AudioBuffer<float>& bandBuffer, float thresholdDb, float ratio,
                      float attackMs, float releaseMs);
    void applyPreset (int index);

    struct Preset
    {
        const char* name;
        std::vector<std::pair<const char*, float>> params;
    };

    static const std::array<Preset, 3> presetBank;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DYNBusLiftAudioProcessor)
};

class DYNBusLiftAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit DYNBusLiftAudioProcessorEditor (DYNBusLiftAudioProcessor&);
    ~DYNBusLiftAudioProcessorEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    DYNBusLiftAudioProcessor& processorRef;

    juce::Colour accentColour;
    gls::ui::GoodluckLookAndFeel lookAndFeel;
    gls::ui::GoodluckHeader headerComponent;
    gls::ui::GoodluckFooter footerComponent;

    juce::Slider lowThreshSlider;
    juce::Slider midThreshSlider;
    juce::Slider highThreshSlider;
    juce::Slider ratioSlider;
    juce::Slider attackSlider;
    juce::Slider releaseSlider;
    juce::Slider mixSlider;
    juce::Slider inputTrimSlider;
    juce::Slider outputTrimSlider;
    juce::ToggleButton bypassButton { "Soft Bypass" };

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
    std::vector<std::unique_ptr<SliderAttachment>> attachments;
    std::vector<std::unique_ptr<ButtonAttachment>> buttonAttachments;
    std::vector<std::unique_ptr<juce::Label>> sliderLabels;

    void initSlider (juce::Slider& slider, const juce::String& label, bool isMacro = false);
    void initToggle (juce::ToggleButton& toggle);
    void layoutLabels();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DYNBusLiftAudioProcessorEditor)
};
