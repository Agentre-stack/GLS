#pragma once

#include <JuceHeader.h>
#include <atomic>
#include "../../DualPrecisionAudioProcessor.h"
#include "../../ui/GoodluckLookAndFeel.h"

class UTLPhaseOrbAudioProcessor : public DualPrecisionAudioProcessor
{
public:
    UTLPhaseOrbAudioProcessor();
    ~UTLPhaseOrbAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    const juce::String getName() const override { return "UTLPhaseOrb"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int index) override { return index == 0 ? juce::String (JucePlugin_Name " 01") : juce::String(); }
    void changeProgramName (int, const juce::String&) override {}

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getValueTreeState() { return apvts; }
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    float getOrbitalPhase() const noexcept { return orbVisual.load(); }

private:
    juce::AudioProcessorValueTreeState apvts;

    double currentSampleRate = 44100.0;
    juce::uint32 lastBlockSize = 0;
    double lfoPhase = 0.0;
    std::atomic<float> orbVisual { 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (UTLPhaseOrbAudioProcessor)
};

class UTLPhaseOrbAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit UTLPhaseOrbAudioProcessorEditor (UTLPhaseOrbAudioProcessor&);
    ~UTLPhaseOrbAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    UTLPhaseOrbAudioProcessor& processorRef;

    juce::Colour accentColour;
    gls::ui::GoodluckLookAndFeel lookAndFeel;
    gls::ui::GoodluckHeader headerComponent;
    gls::ui::GoodluckFooter footerComponent;
    std::unique_ptr<juce::Component> heroVisual;

    juce::Slider widthSlider;
    juce::Slider phaseSlider;
    juce::Slider rateSlider;
    juce::Slider depthSlider;
    juce::Slider tiltSlider;
    juce::Slider mixSlider;
    juce::Slider inputTrimSlider;
    juce::Slider outputTrimSlider;
    juce::ToggleButton bypassButton { "Soft Bypass" };

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    std::vector<std::unique_ptr<SliderAttachment>> sliderAttachments;
    std::vector<std::unique_ptr<ButtonAttachment>> buttonAttachments;
    std::vector<std::unique_ptr<juce::Label>> labels;

    void configureRotarySlider (juce::Slider&, const juce::String&);
    void configureLinearSlider (juce::Slider&, const juce::String&);
    void configureToggle (juce::ToggleButton&, const juce::String&);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (UTLPhaseOrbAudioProcessorEditor)
};
