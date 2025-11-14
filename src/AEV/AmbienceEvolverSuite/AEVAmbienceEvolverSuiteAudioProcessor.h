#pragma once

#include <JuceHeader.h>

class AEVAmbienceEvolverSuiteAudioProcessor : public juce::AudioProcessor
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
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    void triggerProfileCapture();

    juce::AudioProcessorValueTreeState& getValueTreeState() { return apvts; }
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

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

    bool profileCaptureArmed = false;
    int profileSamplesRemaining = 0;
    float capturedNoise = 0.0f;
    float profileAccumulator = 0.0f;
    int profileTotalSamples = 1;

    void ensureStateSize (int numChannels);
    void updateProfileState (float sampleEnv);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AEVAmbienceEvolverSuiteAudioProcessor)
};

class AEVAmbienceEvolverSuiteAudioProcessorEditor : public juce::AudioProcessorEditor,
                                                    private juce::Button::Listener
{
public:
    explicit AEVAmbienceEvolverSuiteAudioProcessorEditor (AEVAmbienceEvolverSuiteAudioProcessor&);
    ~AEVAmbienceEvolverSuiteAudioProcessorEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    AEVAmbienceEvolverSuiteAudioProcessor& processorRef;

    juce::TextButton profileButton { "Capture Profile" };
    juce::Slider ambienceSlider;
    juce::Slider deVerbSlider;
    juce::Slider noiseSlider;
    juce::Slider transientSlider;
    juce::Slider toneMatchSlider;
    juce::Slider hfRecoverSlider;
    juce::Slider outputSlider;

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::vector<std::unique_ptr<SliderAttachment>> attachments;

    void initSlider (juce::Slider& slider, const juce::String& label);
    void buttonClicked (juce::Button* button) override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AEVAmbienceEvolverSuiteAudioProcessorEditor)
};
