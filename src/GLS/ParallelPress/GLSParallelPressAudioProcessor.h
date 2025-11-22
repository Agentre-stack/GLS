#pragma once

#include <JuceHeader.h>
#include <array>
#include "../../DualPrecisionAudioProcessor.h"
#include "../../ui/GoodluckLookAndFeel.h"

class GLSParallelPressAudioProcessor : public DualPrecisionAudioProcessor
{
public:
    GLSParallelPressAudioProcessor();
    ~GLSParallelPressAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "GLSParallelPress"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentPreset; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getValueTreeState() { return apvts; }
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    float getLastReductionDb() const noexcept { return lastReductionDb.load(); }

private:
    juce::AudioProcessorValueTreeState apvts;
    double currentSampleRate = 44100.0;
    juce::uint32 lastBlockSize = 512;
    juce::AudioBuffer<float> dryBuffer;
    juce::AudioBuffer<float> wetBuffer;
    std::atomic<float> lastReductionDb { 0.0f };
    int currentPreset = 0;

    struct ChannelState
    {
        juce::dsp::IIR::Filter<float> hpf;
        juce::dsp::IIR::Filter<float> lpf;
        float envelope = 0.0f;
        float gain = 1.0f;
    };

    struct Preset
    {
        const char* name;
        std::vector<std::pair<const char*, float>> params;
    };

    static const std::array<Preset, 3> presetBank;

    std::vector<ChannelState> channelStates;

    void ensureStateSize();
    void updateFilterCoefficients (ChannelState& state, float hpfFreq, float lpfFreq);
    float computeCompressorGain (float levelDb, float threshDb, float ratio) const;
    static float applyDrive (float sample, float drive);
    void applyPreset (int index);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GLSParallelPressAudioProcessor)
};

class ParallelPressVisual;

class GLSParallelPressAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit GLSParallelPressAudioProcessorEditor (GLSParallelPressAudioProcessor&);
    ~GLSParallelPressAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    GLSParallelPressAudioProcessor& processorRef;

    juce::Colour accentColour;
    gls::ui::GoodluckLookAndFeel lookAndFeel;
    gls::ui::GoodluckHeader headerComponent;
    gls::ui::GoodluckFooter footerComponent;
    std::unique_ptr<ParallelPressVisual> centerVisual;

    juce::Slider driveSlider;
    juce::Slider compThreshSlider;
    juce::Slider compRatioSlider;
    juce::Slider attackSlider;
    juce::Slider releaseSlider;
    juce::Slider hpfWetSlider;
    juce::Slider lpfWetSlider;
    juce::Slider wetLevelSlider;
    juce::Slider dryLevelSlider;
    juce::Slider mixSlider;
    juce::Slider inputTrimSlider;
    juce::Slider outputTrimSlider;
    juce::ToggleButton autoGainButton { "Auto Gain" };
    juce::ToggleButton bypassButton   { "Soft Bypass" };

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

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GLSParallelPressAudioProcessorEditor)
};
