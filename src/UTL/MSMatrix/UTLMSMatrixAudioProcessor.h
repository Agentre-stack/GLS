#pragma once

#include <JuceHeader.h>
#include <atomic>
#include "../../DualPrecisionAudioProcessor.h"
#include "../../ui/GoodluckLookAndFeel.h"

class UTLMSMatrixAudioProcessor : public DualPrecisionAudioProcessor
{
public:
    UTLMSMatrixAudioProcessor();
    ~UTLMSMatrixAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "UTLMSMatrix"; }
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

    float getMidMeter() const noexcept;
    float getSideMeter() const noexcept;
    float getWidthMeter() const noexcept;

private:
    juce::AudioProcessorValueTreeState apvts;
    juce::AudioBuffer<float> dryBuffer;
    juce::dsp::IIR::Filter<float> sideHighPass;
    juce::dsp::IIR::Filter<float> sideLowPass;
    double currentSampleRate = 44100.0;
    float cachedHpf = 120.0f;
    float cachedLpf = 12000.0f;
    std::atomic<float> midMeter  { 0.0f };
    std::atomic<float> sideMeter { 0.0f };
    std::atomic<float> widthMeter{ 0.0f };

    void updateFilters (float hpf, float lpf);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (UTLMSMatrixAudioProcessor)
};

class UTLMSMatrixAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit UTLMSMatrixAudioProcessorEditor (UTLMSMatrixAudioProcessor&);
    ~UTLMSMatrixAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    UTLMSMatrixAudioProcessor& processorRef;

    juce::Colour accentColour;
    gls::ui::GoodluckLookAndFeel lookAndFeel;
    gls::ui::GoodluckHeader headerComponent;
    gls::ui::GoodluckFooter footerComponent;
    std::unique_ptr<juce::Component> heroVisual;

    juce::Slider midGainSlider;
    juce::Slider sideGainSlider;
    juce::Slider widthSlider;
    juce::Slider monoFoldSlider;
    juce::Slider sideHpfSlider;
    juce::Slider sideLpfSlider;
    juce::Slider mixSlider;
    juce::Slider inputTrimSlider;
    juce::Slider outputTrimSlider;
    juce::ToggleButton phaseMidButton { "Phase Mid" };
    juce::ToggleButton phaseSideButton { "Phase Side" };
    juce::ToggleButton bypassButton { "Soft Bypass" };

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
    std::vector<std::unique_ptr<SliderAttachment>> sliderAttachments;
    std::vector<std::unique_ptr<ButtonAttachment>> buttonAttachments;
    std::vector<std::unique_ptr<juce::Label>> labels;

    void configureRotarySlider (juce::Slider&, const juce::String&);
    void configureLinearSlider (juce::Slider&, const juce::String&, bool horizontal);
    void configureToggle (juce::ToggleButton&, const juce::String&);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (UTLMSMatrixAudioProcessorEditor)
};
