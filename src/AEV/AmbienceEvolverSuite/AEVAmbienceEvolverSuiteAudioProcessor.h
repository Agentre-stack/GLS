#pragma once

#include <JuceHeader.h>
#include <array>
#include "../../DualPrecisionAudioProcessor.h"
#include "../../ui/GoodluckLookAndFeel.h"

class AEVAmbienceEvolverSuiteAudioProcessor : public DualPrecisionAudioProcessor
{
public:
    AEVAmbienceEvolverSuiteAudioProcessor();
    ~AEVAmbienceEvolverSuiteAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "AEVAmbienceEvolverSuite"; }
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

    void triggerProfileCapture();

    juce::AudioProcessorValueTreeState& getValueTreeState() { return apvts; }
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    float getLastRmsDb() const noexcept        { return lastRmsDb.load(); }
    float getProfileProgress() const noexcept  { return profileProgress.load(); }
    float getCapturedNoiseLevel() const noexcept { return capturedNoiseValue.load(); }
    bool  isProfileCaptureActive() const noexcept { return profileCaptureArmed; }

private:
    juce::AudioProcessorValueTreeState apvts;
    struct ChannelState
    {
        float noiseEstimate = 0.0f;
        float ambienceState = 0.0f;
        float transientState = 0.0f;
        float toneState = 0.0f;
    };

    std::vector<ChannelState> channelStates;
    double currentSampleRate = 44100.0;
    juce::uint32 lastBlockSize = 512;
    juce::AudioBuffer<float> dryBuffer;

    bool profileCaptureArmed = false;
    int profileSamplesRemaining = 0;
    std::vector<float> profileAccumulators;
    int profileTotalSamples = 1;
    std::array<std::vector<float>, 3> capturedProfiles {};
    std::atomic<float> capturedNoiseValue { 1.0e-6f };
    std::atomic<float> lastRmsDb { -120.0f };
    std::atomic<float> profileProgress { 0.0f };

    void ensureStateSize (int numChannels);
    void updateProfileState (float sampleEnv, int channel);
    void refreshCapturedNoiseSnapshot();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AEVAmbienceEvolverSuiteAudioProcessor)
};

class AmbienceVisualComponent;

class AEVAmbienceEvolverSuiteAudioProcessorEditor : public juce::AudioProcessorEditor,
                                                    private juce::Button::Listener
{
public:
    explicit AEVAmbienceEvolverSuiteAudioProcessorEditor (AEVAmbienceEvolverSuiteAudioProcessor&);
    ~AEVAmbienceEvolverSuiteAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    AEVAmbienceEvolverSuiteAudioProcessor& processorRef;

    juce::Colour accentColour;
    gls::ui::GoodluckLookAndFeel lookAndFeel;
    gls::ui::GoodluckHeader headerComponent;
    gls::ui::GoodluckFooter footerComponent;
    std::unique_ptr<AmbienceVisualComponent> centerVisual;

    juce::Slider ambienceSlider;
    juce::Slider deVerbSlider;
    juce::Slider noiseSlider;
    juce::Slider transientSlider;
    juce::Slider toneMatchSlider;
    juce::Slider hfRecoverSlider;
    juce::Slider mixSlider;
    juce::Slider inputTrimSlider;
    juce::Slider outputTrimSlider;
    juce::ToggleButton bypassButton { "Soft Bypass" };
    juce::TextButton profileButton { "Capture Profile" };
    juce::ComboBox profileSlotBox;

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using ComboAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    std::vector<std::unique_ptr<SliderAttachment>> sliderAttachments;
    std::vector<std::unique_ptr<ButtonAttachment>> buttonAttachments;
    std::unique_ptr<ComboAttachment> profileSlotAttachment;

    struct LabeledSliderRef
    {
        juce::Slider* slider = nullptr;
        juce::Label* label = nullptr;
    };

    std::vector<std::unique_ptr<juce::Label>> sliderLabels;
    std::vector<LabeledSliderRef> labeledSliders;

    void configureSlider (juce::Slider& slider, const juce::String& label, bool isMacro, bool isLinear = false);
    void configureToggle (juce::ToggleButton& toggle);
    void buttonClicked (juce::Button* button) override;
    void layoutLabels();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AEVAmbienceEvolverSuiteAudioProcessorEditor)
};
