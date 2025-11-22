#pragma once

#include <JuceHeader.h>
#include <atomic>
#include <array>
#include "../../DualPrecisionAudioProcessor.h"
#include "../../ui/GoodluckLookAndFeel.h"

class GLSBusGlueAudioProcessor : public DualPrecisionAudioProcessor
{
public:
    GLSBusGlueAudioProcessor();
    ~GLSBusGlueAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "GLSBusGlue"; }
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
    float getLastGainReductionDb() const noexcept { return lastReductionDb.load(); }

private:
    juce::AudioProcessorValueTreeState apvts;
    double currentSampleRate = 44100.0;
    juce::uint32 lastBlockSize = 512;
    juce::AudioBuffer<float> dryBuffer;
    juce::dsp::IIR::Filter<float> sidechainFilter;
    float detectorEnvelope = 0.0f;
    float gainSmoothed = 1.0f;
    std::atomic<float> lastReductionDb { 0.0f };
    int currentPreset = 0;

    struct Preset
    {
        const char* name;
        std::vector<std::pair<const char*, float>> params;
    };

    static const std::array<Preset, 3> presetBank;

    void updateSidechainFilter (float frequency);
    float computeGainDb (float inputLevelDb, float thresholdDb, float ratio, float kneeDb) const;
    void applyPreset (int index);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GLSBusGlueAudioProcessor)
};

class BusGlueVisual;

class GLSBusGlueAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit GLSBusGlueAudioProcessorEditor (GLSBusGlueAudioProcessor&);
    ~GLSBusGlueAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    GLSBusGlueAudioProcessor& processorRef;

    juce::Colour accentColour;
    gls::ui::GoodluckLookAndFeel lookAndFeel;
    gls::ui::GoodluckHeader headerComponent;
    gls::ui::GoodluckFooter footerComponent;
    std::unique_ptr<BusGlueVisual> centerVisual;

    juce::Slider threshSlider;
    juce::Slider ratioSlider;
    juce::Slider attackSlider;
    juce::Slider releaseSlider;
    juce::Slider kneeSlider;
    juce::Slider scHpfSlider;
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

    void configureSlider (juce::Slider& slider, const juce::String& name, bool isMacro, bool isLinear = false);
    void configureToggle (juce::ToggleButton& toggle);
    void layoutLabels();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GLSBusGlueAudioProcessorEditor)
};
