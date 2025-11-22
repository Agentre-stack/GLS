#pragma once

#include <JuceHeader.h>
#include <array>
#include <atomic>
#include "../../DualPrecisionAudioProcessor.h"
#include "../../ui/GoodluckLookAndFeel.h"

class UTLNoiseGenLabAudioProcessor : public DualPrecisionAudioProcessor
{
public:
    UTLNoiseGenLabAudioProcessor();
    ~UTLNoiseGenLabAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    const juce::String getName() const override { return "UTLNoiseGenLab"; }
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
    float getNoiseMeter() const noexcept { return noiseMeter.load(); }

private:
    juce::AudioProcessorValueTreeState apvts;
    juce::AudioBuffer<float> dryBuffer;

    struct NoiseState
    {
        float pink = 0.0f;
        float brown = 0.0f;
    };

    std::array<NoiseState, 2> noiseStates{};
    std::array<juce::dsp::IIR::Filter<float>, 2> lowPassFilters;
    std::array<juce::dsp::IIR::Filter<float>, 2> highPassFilters;
    std::array<juce::LinearSmoothedValue<float>, 2> burstEnvelopes;
    std::array<int, 2> burstCounters{};

    juce::Random random;
    double currentSampleRate = 44100.0;
    juce::uint32 lastBlockSize = 0;
    float lastLowCut = 120.0f;
    float lastHighCut = 12000.0f;
    std::atomic<float> noiseMeter { 0.0f };

    void updateFilters (float lowCutHz, float highCutHz);
    float generateNoise (int channel, int noiseMode, float stereoVariance);
    void refreshBurstTargets (float density, float stereoVariance);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (UTLNoiseGenLabAudioProcessor)
};

class UTLNoiseGenLabAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit UTLNoiseGenLabAudioProcessorEditor (UTLNoiseGenLabAudioProcessor&);
    ~UTLNoiseGenLabAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    UTLNoiseGenLabAudioProcessor& processorRef;

    juce::Colour accentColour;
    gls::ui::GoodluckLookAndFeel lookAndFeel;
    gls::ui::GoodluckHeader headerComponent;
    gls::ui::GoodluckFooter footerComponent;
    std::unique_ptr<juce::Component> heroVisual;

    juce::ComboBox colorSelector;
    juce::Slider noiseLevelSlider;
    juce::Slider mixSlider;
    juce::Slider densitySlider;
    juce::Slider lowCutSlider;
    juce::Slider highCutSlider;
    juce::Slider stereoVarSlider;
    juce::Slider inputTrimSlider;
    juce::Slider outputTrimSlider;
    juce::ToggleButton bypassButton { "Soft Bypass" };

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using ComboAttachment  = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    std::vector<std::unique_ptr<SliderAttachment>> sliderAttachments;
    std::vector<std::unique_ptr<ButtonAttachment>> buttonAttachments;
    std::unique_ptr<ComboAttachment> colorAttachment;
    std::vector<std::unique_ptr<juce::Label>> labels;

    void configureRotarySlider (juce::Slider&, const juce::String&);
    void configureLinearSlider (juce::Slider&, const juce::String&);
    void configureToggle (juce::ToggleButton&, const juce::String&);
    void configureComboBox (juce::ComboBox&, const juce::String&);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (UTLNoiseGenLabAudioProcessorEditor)
};
