#pragma once

#include <JuceHeader.h>

class GLSMixGuardAudioProcessor : public juce::AudioProcessor
{
public:
    GLSMixGuardAudioProcessor();
    ~GLSMixGuardAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "GLSMixGuard"; }
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

    struct ChannelState
    {
        juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> delayLine { 48000 };
        float previousSample = 0.0f;
    };

    std::vector<ChannelState> channelStates;
    float limiterGain = 1.0f;
    float loudnessAccumulator = 0.0f;
    int loudnessSamples = 0;

    void ensureStateSize();
    float measureTruePeak (ChannelState& state, float currentSample, bool tpEnabled);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GLSMixGuardAudioProcessor)
};

class GLSMixGuardAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit GLSMixGuardAudioProcessorEditor (GLSMixGuardAudioProcessor&);
    ~GLSMixGuardAudioProcessorEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    GLSMixGuardAudioProcessor& processorRef;

    juce::Slider ceilingSlider;
    juce::Slider thresholdSlider;
    juce::Slider lookaheadSlider;
    juce::Slider releaseSlider;
    juce::Slider targetLufsSlider;
    juce::ToggleButton tpButton { "True Peak" };

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    std::unique_ptr<SliderAttachment> ceilingAttachment;
    std::unique_ptr<SliderAttachment> thresholdAttachment;
    std::unique_ptr<SliderAttachment> lookaheadAttachment;
    std::unique_ptr<SliderAttachment> releaseAttachment;
    std::unique_ptr<SliderAttachment> targetLufsAttachment;
    std::unique_ptr<ButtonAttachment> tpAttachment;

    void initialiseSlider (juce::Slider& slider, const juce::String& name);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GLSMixGuardAudioProcessorEditor)
};
