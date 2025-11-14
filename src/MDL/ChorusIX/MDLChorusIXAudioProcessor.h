#pragma once

#include <JuceHeader.h>

class MDLChorusIXAudioProcessor : public juce::AudioProcessor
{
public:
    MDLChorusIXAudioProcessor();
    ~MDLChorusIXAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "MDLChorusIX"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 1.5; }

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

    struct ChorusVoice
    {
        juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> delay { 48000 };
        float phase = 0.0f;
    };

    std::vector<std::vector<ChorusVoice>> channelVoices;
    juce::AudioBuffer<float> dryBuffer;
    double currentSampleRate = 44100.0;
    juce::uint32 lastBlockSize = 512;

    void ensureVoiceState (int numChannels, int numVoices);
    void updateToneFilter (float tone);

    juce::dsp::IIR::Filter<float> toneFilters[2];

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MDLChorusIXAudioProcessor)
};

class MDLChorusIXAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit MDLChorusIXAudioProcessorEditor (MDLChorusIXAudioProcessor&);
    ~MDLChorusIXAudioProcessorEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    MDLChorusIXAudioProcessor& processorRef;

    juce::Slider voicesSlider;
    juce::Slider rateSlider;
    juce::Slider depthSlider;
    juce::Slider spreadSlider;
    juce::Slider toneSlider;
    juce::Slider mixSlider;

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::vector<std::unique_ptr<SliderAttachment>> attachments;

    void initSlider (juce::Slider&, const juce::String&);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MDLChorusIXAudioProcessorEditor)
};
