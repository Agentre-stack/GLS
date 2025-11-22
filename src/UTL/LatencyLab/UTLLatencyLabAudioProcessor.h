#pragma once

#include <JuceHeader.h>
#include <vector>
#include <atomic>
#include "../../DualPrecisionAudioProcessor.h"
#include "../../ui/GoodluckLookAndFeel.h"

class UTLLatencyLabAudioProcessor : public DualPrecisionAudioProcessor
{
public:
    UTLLatencyLabAudioProcessor();
    ~UTLLatencyLabAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "UTLLatencyLab"; }
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

    float getLatencyMs() const noexcept;
    float getPingIntervalMs() const noexcept;
    float getPingActivity() const noexcept;

private:
    struct ChannelDelay
    {
        juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> delay { 192000 };
    };

    juce::AudioProcessorValueTreeState apvts;
    std::vector<ChannelDelay> channelDelays;
    juce::AudioBuffer<float> dryBuffer;

    double currentSampleRate = 44100.0;
    juce::uint32 lastBlockSize = 512;
    double delaySpecSampleRate = 0.0;
    juce::uint32 delaySpecBlockSize = 0;
    int lastLatencySamples = 0;

    int pingIntervalSamples = 4410;
    int pingCounterSamples = 4410;
    int pingPulseSamples = 0;
    float pingLevelLinear = 0.0f;
    std::atomic<float> pingActivity { 0.0f };
    std::atomic<bool> pingEnabledFlag { false };

    void ensureStateSize (int numChannels);
    void updateLatency();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (UTLLatencyLabAudioProcessor)
};

class UTLLatencyLabAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit UTLLatencyLabAudioProcessorEditor (UTLLatencyLabAudioProcessor&);
    ~UTLLatencyLabAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    UTLLatencyLabAudioProcessor& processorRef;

    juce::Colour accentColour;
    gls::ui::GoodluckLookAndFeel lookAndFeel;
    gls::ui::GoodluckHeader headerComponent;
    gls::ui::GoodluckFooter footerComponent;
    std::unique_ptr<juce::Component> heroVisual;

    juce::Slider latencySlider;
    juce::Slider pingIntervalSlider;
    juce::Slider pingLevelSlider;
    juce::Slider mixSlider;
    juce::Slider inputTrimSlider;
    juce::Slider outputTrimSlider;
    juce::ToggleButton pingEnableButton { "Ping" };
    juce::ToggleButton bypassButton { "Soft Bypass" };

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
    std::vector<std::unique_ptr<SliderAttachment>> sliderAttachments;
    std::vector<std::unique_ptr<ButtonAttachment>> buttonAttachments;
    std::vector<std::unique_ptr<juce::Label>> labels;

    void configureRotarySlider (juce::Slider&, const juce::String&);
    void configureLinearSlider (juce::Slider&, const juce::String&);
    void configureToggle (juce::ToggleButton&, const juce::String&);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (UTLLatencyLabAudioProcessorEditor)
};
