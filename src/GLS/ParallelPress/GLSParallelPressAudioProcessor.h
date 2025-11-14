#pragma once

#include <JuceHeader.h>

class GLSParallelPressAudioProcessor : public juce::AudioProcessor
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

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getValueTreeState() { return apvts; }
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

private:
    juce::AudioProcessorValueTreeState apvts;
    double currentSampleRate = 44100.0;
    juce::AudioBuffer<float> dryBuffer;
    juce::AudioBuffer<float> wetBuffer;

    struct ChannelState
    {
        juce::dsp::IIR::Filter<float> hpf;
        juce::dsp::IIR::Filter<float> lpf;
        float envelope = 0.0f;
        float gain = 1.0f;
    };

    std::vector<ChannelState> channelStates;

    void ensureStateSize();
    void updateFilterCoefficients (ChannelState& state, float hpfFreq, float lpfFreq);
    float computeCompressorGain (float levelDb, float threshDb, float ratio) const;
    static float applyDrive (float sample, float drive);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GLSParallelPressAudioProcessor)
};

class GLSParallelPressAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit GLSParallelPressAudioProcessorEditor (GLSParallelPressAudioProcessor&);
    ~GLSParallelPressAudioProcessorEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    GLSParallelPressAudioProcessor& processorRef;

    juce::Slider driveSlider;
    juce::Slider compThreshSlider;
    juce::Slider compRatioSlider;
    juce::Slider attackSlider;
    juce::Slider releaseSlider;
    juce::Slider hpfWetSlider;
    juce::Slider lpfWetSlider;
    juce::Slider wetLevelSlider;
    juce::Slider dryLevelSlider;

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::vector<std::unique_ptr<SliderAttachment>> attachments;

    void initialiseSlider (juce::Slider& slider, const juce::String& name);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GLSParallelPressAudioProcessorEditor)
};
